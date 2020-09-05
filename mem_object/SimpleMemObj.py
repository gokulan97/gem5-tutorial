from m5.params import *
from m5.proxy import *
from MemObject import MemObject

class SimpleMemObject(MemObject):
	type = "SimpleMemObj"
	cxx_header = "learning_gem5/mem_object/simple_memobj.hh"

	inst_port = SlavePort("CPU side iport, receives req")
	data_port = SlavePort("CPU side dport, receives req")

	mem_port = MasterPort("Mem side port, sends requests")
