#include "learning_gem5/hello_object.hh"
#include "debug/Hello.hh"
#include <iostream>

HelloObject :: HelloObject(HelloObjectParams *params)
	: SimObject(params), event([this]{processEvent();}, name())
{
	count = 10;
	DPRINTF(Hello, "Hello from simobject\n");
}

void HelloObject :: startup()
{
	schedule(event, 100);
}

void HelloObject :: processEvent()
{
	DPRINTF(Hello, "Processing event, remaining: %d\n", count--);
	if(count > 0)
		schedule(event, curTick()+100);
}

HelloObject* HelloObjectParams::create()
{
	return new HelloObject(this);
}
