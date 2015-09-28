#include "proxy.h"

InputHandlerProxy::~InputHandlerProxy()
{
	//TODO
}

void InputHandlerProxy::add(u32 port, InputHandler* handler)
{
	if(handler != NULL)
	{
		this->handlers.insert(std::pair<u32, InputHandler*>(port, handler));
	}
}

void InputHandlerProxy::handle(u32 port)
{
	std::pair <InputHandlerStore::iterator, InputHandlerStore::iterator> range;
	range = this->handlers.equal_range(port);

	for (InputHandlerStore::iterator it = range.first; it != range.second; ++it)
	{
		it->second->handle();
	}
}