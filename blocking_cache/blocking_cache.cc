#include "base/random.hh"
#include "learning_gem5/blocking_cache/blocking_cache.hh"
#include "debug/BCache.hh"
#include "sim/system.hh"

BlockingCache::BlockingCache(BlockingCacheParams *params) :
	MemObject(params),
	latency(params->latency),
	blockSize(params->system->cacheLineSize()),
	capacity(params->size / blockSize),
	memPort(params->name + ".mem_side", this),
	blocked(false),
	pendingPkt(nullptr),
	waitingPortID(-1)
	{
		for(int i=0; i<params->port_cpu_side_connection_count; i++)
		{
			cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
		}
	}

Port& BlockingCache::getPort(const std::string& if_name, PortID idx)
{
	if(if_name == "mem_side")
		return memPort;
	else if(if_name == "cpu_side" && idx < cpuPorts.size())
		return cpuPorts[idx];
	else
		return SimObject::getPort(if_name, idx);
}

void BlockingCache::accessTiming(PacketPtr pkt)
{
	bool hit = accessFunctional(pkt);
	if(hit)
	{
		pkt->makeResponse();
		sendResponse(pkt);
	}
	else
	{
		//cache miss, forward to mem port
		pendingPkt = pkt;
		Addr addr = pkt->getAddr();
		Addr block_addr = pkt->getBlockAddr(blockSize);
		unsigned size = pkt->getSize();
		if(addr == block_addr && size == blockSize) //cache miss, request size equals blockSize
		{
			DPRINTF(BCache, "Forwarding packet\n");
			memPort.sendPacket(pkt);
		}
		else //cache miss, request size < blockSize, creating new pkt without modifying old one
		{		 //and sending the new packet to memPort, will send the old packet to CPU side once
				 //response is received
			DPRINTF(BCache, "Req size less than block size, upgrading\n");
			panic_if(addr - block_addr + size > blockSize, "Req cannot span multiple blocks\n");

			assert(pkt->needsResponse());

			MemCmd cmd;
			if(pkt->isWrite() || pkt->isRead())
				cmd = MemCmd::ReadReq;
			else
				panic("Unknown packet type\n");

			PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
			new_pkt->allocate();
			pendingPkt = pkt;

			memPort.sendPacket(new_pkt);
		}
	}
}

bool BlockingCache::accessFunctional(PacketPtr pkt)
{
	Addr block_addr = pkt->getBlockAddr(blockSize);
	auto it = cacheStore.find(block_addr);

	if(it != cacheStore.end())
	{
		if(pkt->isWrite())
			pkt->writeDataToBlock(it->second, blockSize);
		else if(pkt->isRead())
			pkt->setDataFromBlock(it->second, blockSize);
		else
			DPRINTF(BCache, "Unknown packet type!");
		return true;
	}
	return false;
}

void BlockingCache::insert(PacketPtr pkt)
{
	if(cacheStore.size() >= capacity)
	{
		int bucket, bucket_size;
		do
		{
			bucket = random_mt.random(0, (int)cacheStore.bucket_count()-1);
		} while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0);
		
		auto block = std::next(cacheStore.begin(bucket), random_mt.random(0, bucket_size-1));

		RequestPtr req(new Request(block->first, blockSize, 0, 0));
		PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
		new_pkt->dataDynamic(block->second);

		DPRINTF(BCache, "Write back dirty packet: %s\n", pkt->print());
		memPort.sendTimingReq(new_pkt);

		cacheStore.erase(block->first);
	}

	uint8_t *data = new uint8_t[blockSize];
	cacheStore[pkt->getAddr()] = data; //insert pointer into hashmap

	pkt->writeDataToBlock(data, blockSize); //copies data in pointer to new cache block
}

void BlockingCache::sendResponse(PacketPtr pkt)
{
	int port = waitingPortID;

	blocked = false;
	waitingPortID = -1;

	cpuPorts[port].sendPacket(pkt);
	for(auto port: cpuPorts)
		port.trySendRetry();
}

AddrRangeList CPUSidePort::getAddrRanges() const
{
	return owner->getAddrRanges();
}

void CPUSidePort::recvFunctional(PacketPtr pkt)
{
	owner->handleFunctional(pkt);
}

void BlockingCache::handleFunctional(PacketPtr pkt)
{
	memPort.sendFunctional(pkt);
}

AddrRangeList BlockingCache::getAddrRanges() const
{
	return memPort.getAddrRanges();
}

void MemSidePort::recvRangeChange()
{
	owner->sendRangeChange();
}

void BlockingCache::sendRangeChange()
{
	for(auto port: cpuPorts)
		port.sendRangeChange();
}

bool CPUSidePort::recvTimingReq(PacketPtr pkt)
{
	if(blockedPacket || needRetry)
	{
		DPRINTF(BCache, "Already request in flight, blocking current request\n");
		needRetry = true;
		return false;
	}
	else if(!owner->handleRequest(pkt, id))
	{
		DPRINTF(BCache, "Owner couldn't handle current request\n");
		needRetry = true;
		return false;
	}
	return true;
}

bool BlockingCache::handleRequest(PacketPtr pkt, int portID)
{
	if(blocked)
		return false;
	
	DPRINTF(BCache, "Got request for addr: %x\n", pkt->getAddr());
	blocked = true;
	waitingPortID = portID;
	
	schedule(new AccessEvent(this, pkt), clockEdge(latency));

	return true;
}

void MemSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "Don't send when receiver blocked!");
	if(!sendTimingReq(pkt))
		blockedPacket = pkt;
}

void MemSidePort::recvReqRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr ptr = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(ptr);
}

bool MemSidePort::recvTimingResp(PacketPtr pkt)
{
	return owner->handleResponse(pkt);
}

bool BlockingCache::handleResponse(PacketPtr pkt)
{
	assert(blocked);
	DPRINTF(BCache, "Out resp for addr: %x\n", pkt->getAddr());
	insert(pkt); // received response from memory, now inserting it into cache

	if(pendingPkt != nullptr)
	{
		accessFunctional(pendingPkt); //accessing the cache after data response has been inserted
		pendingPkt->makeResponse(); // converting the request to a response type
		delete pkt;
		pkt = pendingPkt;
		pendingPkt = nullptr;
	}

	sendResponse(pkt); //returning resp to the host CPU
	return true;
}

void CPUSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "dont send when blocked\n");
	if(!sendTimingResp(pkt))
		blockedPacket = pkt;
}

void CPUSidePort::recvRespRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr ptr = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(ptr);
}

void CPUSidePort::trySendRetry()
{
	if(needRetry && blockedPacket == nullptr)
	{
		needRetry = false;
		DPRINTF(BCache, "sending retry req for id: %d\n", id);
		sendRetryReq();
	}
}

BlockingCache* BlockingCacheParams::create()
{
	return new BlockingCache(this);
}
