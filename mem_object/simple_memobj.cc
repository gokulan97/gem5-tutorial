
#include "learning_gem5/mem_object/simple_memobj.hh"
#include "debug/SimpleMemObj.hh"

SimpleMemObj::SimpleMemObj(SimpleMemObjParams *params) :
	SimObject(params),
	instPort(params->name + ".inst_port", this),
	dataPort(params->name + ".data_port", this),
	memPort(params->name + ".mem_port", this),
	blocked(false)
	{}

Port& SimpleMemObj::getPort(const std::string& if_name, PortID idx)
{
	if(if_name == "mem_port")
		return memPort;
	else if(if_name == "inst_port")
		return instPort;
	else if(if_name == "data_port")
		return dataPort;
	else
		return SimObject::getPort(if_name, idx);
}

AddrRangeList CPUSidePort::getAddrRanges() const
{
	return owner->getAddrRanges();
}

void CPUSidePort::recvFunctional(PacketPtr pkt)
{
	owner->handleFunctional(pkt);
}

void SimpleMemObj::handleFunctional(PacketPtr pkt)
{
	memPort.sendFunctional(pkt);
}

AddrRangeList SimpleMemObj::getAddrRanges() const
{
	return memPort.getAddrRanges();
}

void MemSidePort::recvRangeChange()
{
	owner->sendRangeChange();
}

void SimpleMemObj::sendRangeChange()
{
	instPort.sendRangeChange();
	dataPort.sendRangeChange();
}

bool CPUSidePort::recvTimingReq(PacketPtr pkt)
{
	if(!owner->handleRequest(pkt))
	{
		needRetry = true;
		return false;
	}
	return true;
}

bool SimpleMemObj::handleRequest(PacketPtr pkt)
{
	if(blocked)
		return false;
	
	DPRINTF(SimpleMemObj, "Got request for addr: %x\n", pkt->getAddr());
	blocked = true;
	memPort.sendPacket(pkt);
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

bool SimpleMemObj::handleResponse(PacketPtr pkt)
{
	assert(blocked);
	DPRINTF(SimpleMemObj, "Out resp for addr: %x\n", pkt->getAddr());

	blocked = false;

	if(pkt->req->isInstFetch())
		instPort.sendPacket(pkt);
	else
		dataPort.sendPacket(pkt);
	
	instPort.trySendRetry();
	dataPort.trySendRetry();

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
		DPRINTF(SimpleMemObj, "sending retry req for id: %d\n", id);
		sendRetryReq();
	}
}

SimpleMemObj* SimpleMemObjParams::create()
{
	return new SimpleMemObj(this);
}
