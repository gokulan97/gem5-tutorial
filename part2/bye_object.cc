#include "learning_gem5/part2/bye_object.hh"
#include "debug/Hello.hh"
#include "sim/sim_exit.hh"

void ByeObject::processEvent()
{
	DPRINTF(Hello, "Processing event\n");
	fillBuffer();
}

void ByeObject::fillBuffer()
{
	assert(message.length() > 0);

	int bytes_copied = 0;
	for(auto it = message.begin(); it < message.end() && bufferUsed < bufferSize-1;
			bufferUsed++, it++, bytes_copied++)
	{
		buffer[bufferUsed] = *it;
	}

	DPRINTF(Hello, "Copied %d bytes to buffer, %d used\n", bytes_copied, bufferUsed);

	if(bufferUsed < bufferSize - 1)
	{
		DPRINTF(Hello, "Scheduling another event\n");
		schedule(event, curTick() + bandwidth * bytes_copied);
	}
	else
	{
		DPRINTF(Hello, "Exiting sim loop @ %d\n", curTick() + bandwidth *	bytes_copied);
		exitSimLoop(buffer, 0, curTick() + bandwidth * bytes_copied);
	}
}

ByeObject::ByeObject(ByeObjectParams* params) : SimObject(params), event(*this)
{
	bufferSize = params->buffer_size;
	buffer = new char[bufferSize];
	bandwidth = params -> write_bw;
	bufferUsed = 0;
}

ByeObject::~ByeObject()
{
	delete[] buffer;
}

void ByeObject::sayBye(std::string msg)
{
	message = "adios " + msg;
	fillBuffer();
}

ByeObject* ByeObjectParams::create()
{
	return new ByeObject(this);
}
