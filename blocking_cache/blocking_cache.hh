#ifndef __LEARNING_GEM5_BLOCKING_CACHE_BLOCKING_CACHE_HH__
#define __LEARNING_GEM5_BLOCKING_CACHE_BLOCKING_CACHE_HH__

#include <vector>
#include <unordered_map>

#include "mem/port.hh"
#include "mem/mem_object.hh"
#include "sim/sim_object.hh"
#include "params/BlockingCache.hh"

class CPUSidePort : public SlavePort
{
	private:
		BlockingCache *owner;
		PacketPtr blockedPacket;
		bool needRetry;
		int id;

	public:
		CPUSidePort(const std::string& name, int id, BlockingCache* owner) :
			SlavePort(name, (SimObject*) owner), owner(owner), blockedPacket(nullptr),
			needRetry(false), id(id)
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
		BlockingCache *owner;
		PacketPtr blockedPacket;

	public:
		MemSidePort(const std::string &name, BlockingCache* owner) :
			MasterPort(name, (SimObject*) owner), owner(owner), blockedPacket(nullptr)
			{}
		void sendPacket(PacketPtr pkt);

	protected:
		bool recvTimingResp(PacketPtr pkt) override;
		void recvReqRetry() override;
		void recvRangeChange() override;
};

class BlockingCache : public MemObject
{
	private:
		const Cycles latency;
		const unsigned blockSize;
		const unsigned capacity;

		std::vector<CPUSidePort> cpuPorts;
		
		MemSidePort memPort;
		
		bool blocked;
		PacketPtr pendingPkt;
		int waitingPortID;
		std::unordered_map<Addr, uint8_t*> cacheStore;

	public:
		BlockingCache(BlockingCacheParams *params);

		Port &getPort(const std::string &if_name, PortID idx = InvalidPortID) override;

		bool handleRequest(PacketPtr pkt, int port_id);
		bool handleResponse(PacketPtr pkt);
		void handleFunctional(PacketPtr pkt);
		AddrRangeList getAddrRanges() const;

		void sendRangeChange();
		void sendResponse(PacketPtr pkt);
		void accessTiming(PacketPtr pkt);
		bool accessFunctional(PacketPtr pkt);
		void insert(PacketPtr pkt);
};

class AccessEvent : public Event
{
	private:
		BlockingCache *cache;
		PacketPtr pkt;
	
	public:
		AccessEvent(BlockingCache *cache, PacketPtr pkt) :
			Event(Default_Pri, AutoDelete), cache(cache), pkt(pkt)
			{}

		void process() override
		{
			cache->accessTiming(pkt);
		}
};

#endif
