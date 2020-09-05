from m5.params import *
from m5.SimObject import SimObject

class HelloObject(SimObject):
	type = 'HelloObject' # Name of C++ class which is wrapped in this SimObject
	cxx_header = 'learning_gem5/part2/hello_object.hh' # Class type is declared here

	fire_count = Param.Int(10, "Number of times event should fire")

	bye_object = Param.ByeObject("A Bye Object")

class ByeObject(SimObject):
	type = 'ByeObject'
	cxx_header = 'learning_gem5/part2/bye_object.hh'

	buffer_size = Param.MemorySize('1kB', "Size of buffer")
	write_bw = Param.MemoryBandwidth('100MB/s', "Available bandwidth")
