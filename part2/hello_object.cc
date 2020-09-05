#include "learning_gem5/part2/hello_object.hh"

#include "debug/Hello.hh"

HelloObject :: HelloObject(HelloObjectParams *params)
	: SimObject(params), event([this]{processEvent();}, name()),
		obj(*params->bye_object)
{
	count = params->fire_count;
	DPRINTF(Hello, "Hello from simobject\n");
	//panic_if(!object, "Bye Object must be non NULL");
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

	if(count == 0)
		obj.sayBye(this->name());
}

HelloObject* HelloObjectParams::create()
{
	return new HelloObject(this);
}
