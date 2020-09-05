#ifndef __LEARNING_GEM5_HELLO_OBJECT_HH__
#define __LEARNING_GEM5_HELLO_OBJECT_HH__

#include "learning_gem5/part2/bye_object.hh"
#include "params/HelloObject.hh"
#include "sim/sim_object.hh"

class HelloObject : public SimObject
{
	private:
		void processEvent();

		EventFunctionWrapper event;
		int count;
		
		ByeObject& obj;
	public:
		HelloObject(HelloObjectParams *params);

		void startup();
};

#endif
