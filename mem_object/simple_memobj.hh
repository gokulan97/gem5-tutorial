#ifndef __LEARNING_GEM5_MEM_OBJECT_SIMPLE_MEMOBJ_HH__
#define __LEARNING_GEM5_MEM_OBJECT_SIMPLE_MEMOBJ_HH__

#include <vector>

#include "mem/port.hh"
#include "sim/sim_object.hh"
#include "params/SimpleMemObj.hh"

class CPUSidePort : public SlavePort
{
	private:
		SimpleMemObj *owner;
		PacketPtr blockedPacket;
		bool needRetry;

	public:
		CPUSidePort(const std::string& name, SimpleMemObj* owner) :
			SlavePort(name, (SimObject*) owner), owner(owner), blockedPacket(nullptr),
			needRetry(false)
			{}

		AddrRangeList getAddrRanges() const override;
		void sendPacket(PacketPtr pkt);
		void trySendRetry();

	protected:
		Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic	unimplemented");}
		void recvFunctional(PacketPtr pkt) override;
		bool recvTimingReq(PacketPtr pkt) override;
		void recvRespRetry() override;
};

class MemSidePort : public MasterPort
{
	private:
		SimpleMemObj *owner;
		PacketPtr blockedPacket;

	public:
		MemSidePort(const std::string &name, SimpleMemObj* owner) :
			MasterPort(name, (SimObject*) owner), owner(owner), blockedPacket(nullptr)
			{}
		void sendPacket(PacketPtr pkt);

	protected:
		bool recvTimingResp(PacketPtr pkt) override;
		void recvReqRetry() override;
		void recvRangeChange() override;
};

class SimpleMemObj: public SimObject
{
	private:
		CPUSidePort instPort;
		CPUSidePort dataPort;

		MemSidePort memPort;
		
		bool blocked;
		
	public:
		SimpleMemObj(SimpleMemObjParams *params);

		Port &getPort(const std::string &if_name, PortID idx = InvalidPortID) override;

		bool handleRequest(PacketPtr pkt);
		bool handleResponse(PacketPtr pkt);
		void handleFunctional(PacketPtr pkt);
		AddrRangeList getAddrRanges() const;


		void sendRangeChange();
};

#endif
