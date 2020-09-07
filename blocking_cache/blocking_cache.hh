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

		//The object that owns this port
		BlockingCache *owner;

		//The packet which is being processed - sent to memPort for response or currently being processed
		PacketPtr blockedPacket;

		//If the master sends a request when the cache is busy, it is recorded here. Once a packet is
		//processed, sendReqRetry() is called to inform the master to try sending req again
		bool needRetry;

		//The ID of the port
		int id;

	public:
		CPUSidePort(const std::string& name, int id, BlockingCache* owner) :
			SlavePort(name, (SimObject*) owner), //Parent Class' constructor
			owner(owner),
			blockedPacket(nullptr),
			needRetry(false),
			id(id)
			{}

		//Function which sends addr range of the slave ports to the master
		AddrRangeList getAddrRanges() const override;

		//Try to send response back to the master port, can be called multiple times on the same packet
		//depending on the master's availability
		void sendPacket(PacketPtr pkt);

		//call sendReqRetry() if the variable needRetry is set. This function is called after the response
		//packet is successfully sent to the master port
		void trySendRetry();

	protected:
		//not implemented
		Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic	unimplemented");}

		//In case of a functional query with a packet, forward it to owner
		void recvFunctional(PacketPtr pkt) override;

		//Receive request from master port, can process optionally
		bool recvTimingReq(PacketPtr pkt) override;

		//called when the master port invokes sendRespRetry from its side, indicating it can now receive response
		void recvRespRetry() override;
};

class MemSidePort : public MasterPort
{
	private:
		//The Cache object this port belongs to
		BlockingCache *owner;
		//store packet if the slave port is busy to accept packet
		PacketPtr blockedPacket;

	public:
		MemSidePort(const std::string &name, BlockingCache* owner) :
			MasterPort(name, (SimObject*) owner), //Constructor of Parent class
			owner(owner),
			blockedPacket(nullptr)
			{}
		//Attempt to send packet to slave port, might get called multiple times for the same packet
		void sendPacket(PacketPtr pkt);

	protected:
		// receive response packet from the slave port
		bool recvTimingResp(PacketPtr pkt) override;
		// this function is called by slave port for the master port to reattempt sending request,
		// which failed earlier. The request is stored in blockedPacket variable.
		void recvReqRetry() override;
		// Receive changes in address ranges from slave port, forwarded to owner
		void recvRangeChange() override;
};

class BlockingCache : public MemObject
{
	private:
		//Cache access latency (tag+data)
		const Cycles latency;
		//Cache block size
		const unsigned blockSize;
		//number of sets
		const unsigned capacity;
		//slave ports to connect to CPU, to receive requests for instruction and data memory
		std::vector<CPUSidePort> cpuPorts;
		//master port to connect to main memory, to send requests & receive mem response.
		MemSidePort memPort;
		
		//once blocked variable is set to true, it officially marks the end of CPUSidePort handling
		//the packet. The slave port(s) will now wait for response back from the cache object
		//unset only when the response packet has been accepted by the CPUSidePort
		bool blocked;

		//Used to store the current request packet if the request results in a cache miss. The variable
		//is cleared when response for the memory request arrives from memory, is inserted into the cache
		//While unset happens after another functional access from the cache, it does not incur any delay
		//since the response as well can be directly forwarded to the CPUSidePort
		PacketPtr pendingPkt;

		//ID of the CPUSidePort from which the pending request was rceived, Used to forward response to
		//appropriate port
		int waitingPortID;

		//Structure to store cached data
		std::unordered_map<Addr, uint8_t*> cacheStore;

	public:
		BlockingCache(BlockingCacheParams *params);

		Port &getPort(const std::string &if_name, PortID idx = InvalidPortID) override;

		//Called by the CPUSidePort(s) to send request. This in turn schedules an event after latency
		//cycles to perform cache access
		bool handleRequest(PacketPtr pkt, int port_id);

		//called by the MemSidePort to send response back. This inserts the block into the cache, prepares
		//the response packet from original request packet, and sends response to CPUSidePort
		bool handleResponse(PacketPtr pkt);

		//Functional Data access from the CPU is serviced using this function
		void handleFunctional(PacketPtr pkt);

		//address range of memory port is queried using this function
		AddrRangeList getAddrRanges() const;

		//this function is used by MemSidePort to push changes in memory address ranges
		void sendRangeChange();

		//called by handleResponse(), sends response to CPUSidePort
		void sendResponse(PacketPtr pkt);
		//after incurring latency delay, this function is called by event handler. Resolves a request
		//into HIT or MISS. Resonds back in case of hit or forwards request to MemSidePort, pendingPkt
		//variable is set in which case
		void accessTiming(PacketPtr pkt);
		//Functional access of the data array. Performs Read/Write in case of HIT and returns true. If
		//MISS, returns false
		bool accessFunctional(PacketPtr pkt);
		//helper function to insert data present in pkt into the cache
		void insert(PacketPtr pkt);
};

class AccessEvent : public Event
{
	private:
		//the cache in which this event occurs
		BlockingCache *cache;
		//the packet which is processed in this event
		PacketPtr pkt;
	
	public:
		AccessEvent(BlockingCache *cache, PacketPtr pkt) :
			Event(Default_Pri, AutoDelete), //AutoDelete is used to automatically free the event once processed
			cache(cache), 
			pkt(pkt)
			{}

		//Once this event is scheduled, this function gets called after latency delay. Performs the actual
		//cache access
		void process() override
		{
			cache->accessTiming(pkt);
		}
};

#endif
