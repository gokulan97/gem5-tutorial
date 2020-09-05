#include "learning_gem5/hello_object.hh"

#include <iostream>

HelloObject :: HelloObject(HelloObjectParams *params)
	: SimObject(params)
{
	std::cout << "Hello from simobject" << std::endl;
}

HelloObject* HelloObjectParams::create()
{
	return new HelloObject(this);
}
