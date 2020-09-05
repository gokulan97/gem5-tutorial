#ifndef __LEARNING_GEM5_PART2_BYE_OBJECT_HH__
#define __LEARNING_GEM5_PART2_BYE_OBJECT_HH__

#include <string>

#include "params/ByeObject.hh"
#include "sim/sim_object.hh"

class ByeObject: public SimObject
{
	private:
		void processEvent();

		void fillBuffer();

		EventWrapper<ByeObject, &ByeObject::processEvent> event;

		float bandwidth;
		int bufferSize;

		char* buffer;

		std::string message;

		int bufferUsed;

	public:
		ByeObject(ByeObjectParams* params);
		~ByeObject();

		void sayBye(std::string msg);
};

#endif
