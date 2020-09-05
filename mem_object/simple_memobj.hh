#ifndef __LEARNING_GEM5_MEM_OBJECT_SIMPLE_MEMOBJ_HH__
#define __LEARNING_GEM5_MEM_OBJECT_SIMPLE_MEMOBJ_HH__

#include "params/SimpleMemObj.hh"

class SimpleMemObj: public MemObject
{
	private:
		CPUSidePort instPort;
		CPUSidePort dataPort;

		bool blocked;

		MemSidePort memPort;

	public:
		SimpleMemObj(SimpleMemObjParams *params);

		BaseMasterPort& getMasterPort(const std::string& if_name, PortID idx =
		InvalidPortID) override;

		BaseSlavePort& getSlavePort(const std::string& if_name, PortID idx =
		InvalidPortID) override;

		void handleFunctional(PacketPtr pkt);
		AddrRangeList getAddrRanges() const;
};

class CPUSidePort : public SlavePort
{
	private:
		SimpleMemObj *owner;
		PacketPtr blockedPacket;
		bool needRetry;

	public:
		CPUSidePort(const std::string& name, SimpleMemObj owner) :
			SlavePort(name, owner), owner(owner)
			{}

		AddrRangeList getAddrRanges() const override;

	protected:
		Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic	unimplemented");}
		void recvFunctional(PacketPtr pkt) override;
		bool recvTimingReq(PacketPtr pkt) override;
		bool recvRespRetry() override;
};

class MemSidePort : public MasterPort
{
	private:
		SimpleMemObj *owner;
		PacketPtr blockedPacket;

	public:
		MemSidePort(const std::string &name, SimpleMemObj owner) :
			MasterPort(name, owner), owner(owner)
			{}

	protected:
		bool recvTimingResp(PacketPtr pkt) override;
		void recvReqRetry() override;
		void recvRangeChange() override;
		void sendPacket(PacketPtr pkt);
};

#endif
