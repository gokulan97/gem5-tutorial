from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject
from MemObject import MemObject

class BlockingCache(MemObject):
	type = 'BlockingCache'
	cxx_header = 'learning_gem5/blocking_cache/blocking_cache.hh'

	cpu_side = VectorSlavePort("CPU Side port, receives requests")
	mem_side = MasterPort("Mem Side port, receives responses")

	latency = Param.Cycles(1, "Cache Hit Latency or miss resolution latency")

	size = Param.MemorySize('16kB', "Size of cache memory")

	system = Param.System(Parent.any, "The system this cache is part of")
