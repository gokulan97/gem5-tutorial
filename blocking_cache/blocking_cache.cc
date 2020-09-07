#include "base/random.hh"
#include "learning_gem5/blocking_cache/blocking_cache.hh"
#include "debug/BCache.hh"
#include "sim/system.hh"

BlockingCache::BlockingCache(BlockingCacheParams *params) :
	MemObject(params), //Constructor invocation of parent class
	latency(params->latency), //init of class members
	blockSize(params->system->cacheLineSize()),
	capacity(params->size / blockSize),
	memPort(params->name + ".mem_side", this), //memory side master port
	blocked(false),
	pendingPkt(nullptr),
	waitingPortID(-1)
	{
		for(int i=0; i<params->port_cpu_side_connection_count; i++)
		{
			cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this); //cpu side slave port
		}
	}

Port& BlockingCache::getPort(const std::string& if_name, PortID idx)
{
	if(if_name == "mem_side")//single memory side port
		return memPort;
	else if(if_name == "cpu_side" && idx < cpuPorts.size()) // multiple CPU side ports, return appropriate port
		return cpuPorts[idx];
	else
		return SimObject::getPort(if_name, idx);
}

void BlockingCache::accessTiming(PacketPtr pkt)
{
	bool hit = accessFunctional(pkt);//functional access, returns hit or miss; Performs appropriate cache operation
	if(hit)
	{
		pkt->makeResponse();//convert Req packet to Resp
		sendResponse(pkt);//send packet to CPU side
	}
	else
	{
		//cache miss
		pendingPkt = pkt; //store the current req, later packets will get blocked

		Addr addr = pkt->getAddr();
		Addr block_addr = pkt->getBlockAddr(blockSize);
		unsigned size = pkt->getSize(); 
		
		//if the request has the same size as that of cache block size, same packet can be forwarded
		if(addr == block_addr && size == blockSize)
		{
			DPRINTF(BCache, "Forwarding packet\n");
			memPort.sendPacket(pkt);
		}
		else //cache miss, request size < blockSize, creating new pkt without modifying old one
		{		 //and sending the new packet to memPort, will send the old packet to CPU side once
				 //response is received
			DPRINTF(BCache, "Req size less than block size, upgrading\n");

			//the case when a single request spans multiple cache blocks, not allowed
			panic_if(addr - block_addr + size > blockSize, "Req cannot span multiple blocks\n");

			assert(pkt->needsResponse());

			//construct the new packet to be sent to the memory
			MemCmd cmd;
			if(pkt->isWrite() || pkt->isRead())//a write request will read and make it dirty
				cmd = MemCmd::ReadReq;
			else
				panic("Unknown packet type\n");

			PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
			new_pkt->allocate();
			pendingPkt = pkt;

			//send the newly constructed packet to memory
			memPort.sendPacket(new_pkt);
		}
	}
}

bool BlockingCache::accessFunctional(PacketPtr pkt)
{
	Addr block_addr = pkt->getBlockAddr(blockSize);
	auto it = cacheStore.find(block_addr);

	if(it != cacheStore.end())//cache hit
	{
		if(pkt->isWrite())//write request: copy data from pkt to cacheStorage
			pkt->writeDataToBlock(it->second, blockSize);
		else if(pkt->isRead())// read request: copy data from cacheStorage to pkt
			pkt->setDataFromBlock(it->second, blockSize);
		else
			DPRINTF(BCache, "Unknown packet type!");
		return true;
	}
	return false;//cache miss
}

void BlockingCache::insert(PacketPtr pkt)
{
	if(cacheStore.size() >= capacity)//cache full, evict block
	{
		int bucket, bucket_size;
		do
		{
			bucket = random_mt.random(0, (int)cacheStore.bucket_count()-1);
		} while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0);
		
		auto block = std::next(cacheStore.begin(bucket), random_mt.random(0, bucket_size-1));

		//prepare new request packet to write back data to memory, resulting from eviction
		RequestPtr req(new Request(block->first, blockSize, 0, 0));
		PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
		new_pkt->dataDynamic(block->second);

		DPRINTF(BCache, "Write back dirty packet: %s\n", pkt->print());
		//send dirty packet to memory
		memPort.sendTimingReq(new_pkt);

		//delete packet from cacheStorage
		cacheStore.erase(block->first);
	}

	//insert packet into cacheStorage. The map contains only pointer, space for data is allocated
	//dynamically, and will be cleared once it is written back to main memory or evicted cleanly
	uint8_t *data = new uint8_t[blockSize];
	cacheStore[pkt->getAddr()] = data; //insert pointer into hashmap

	pkt->writeDataToBlock(data, blockSize); //copies data in pointer to new cache block
}

void BlockingCache::sendResponse(PacketPtr pkt)
{
	int port = waitingPortID;

	blocked = false;
	waitingPortID = -1;
	//data response available in pkt, unblock and send packet to CPUSidePort
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
	//CPUSidePort already has a request, can't handle another, so blocked.
	//needRetry set so that sendReqRetry() can be called appropriately
	if(blockedPacket || needRetry)
	{
		DPRINTF(BCache, "Already request in flight, blocking current request\n");
		needRetry = true;
		return false;
	}
	else if(!owner->handleRequest(pkt, id))//CPUSidePort free, but owner can't handle request. block it
	{//the packet is conditionally handled, in which case this block doesn't get executed
		DPRINTF(BCache, "Owner couldn't handle current request\n");
		needRetry = true;
		return false;
	}
	return true;//request is handled
}

bool BlockingCache::handleRequest(PacketPtr pkt, int portID)
{
	if(blocked)//new requests blocked
		return false;
	
	DPRINTF(BCache, "Got request for addr: %x\n", pkt->getAddr());
	blocked = true;
	waitingPortID = portID;//request accepted, block future requests
	
	schedule(new AccessEvent(this, pkt), clockEdge(latency));//schedule cache access after latency delay

	return true;
}

void MemSidePort::sendPacket(PacketPtr pkt)
{
	//the case when previous req sent by owner isn't handled yet, wait!
	panic_if(blockedPacket != nullptr, "Don't send when receiver blocked!");

	//request conditionally accepted by MemSidePort
	if(!sendTimingReq(pkt))
		blockedPacket = pkt;
}

void MemSidePort::recvReqRetry()
{
	//slave free now, can reattempt req, but req should exist in the first place.
	assert(blockedPacket != nullptr);

	//port tries to send request to memory
	PacketPtr ptr = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(ptr);
}

bool MemSidePort::recvTimingResp(PacketPtr pkt)
{
	return owner->handleResponse(pkt);//fwd response from memory for packet which blocks to owner
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
	//the case hen the previous response from owner isn't handled yet, wait!
	panic_if(blockedPacket != nullptr, "dont send when blocked\n");

	//port can send response from owner to cpu, conditionally. block if CPU busy
	if(!sendTimingResp(pkt))
		blockedPacket = pkt;
}

void CPUSidePort::recvRespRetry()
{
	//signal to retry sending response, which failed earlier because owner was busy.
	//there should be a packet waiting to be sent
	assert(blockedPacket != nullptr);

	PacketPtr ptr = blockedPacket;
	blockedPacket = nullptr;

	//send packet to CPU
	sendPacket(ptr);
}

void CPUSidePort::trySendRetry()
{
	//call sendRetryReq to inform master to send request again, for
	//1. request already failed (needRetry set)
	//2. no pending requests (blockedPacket is null)
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
