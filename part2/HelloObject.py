from m5.params import *
from m5.SimObject import SimObject

class HelloObject(SimObject):
	type = 'HelloObject' # Name of C++ class which is wrapped in this SimObject
	cxx_header = 'learning_gem5/hello_object.hh' # Class type is declared here

