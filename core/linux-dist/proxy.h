#pragma once
#include <map>
#include "types.h"
#include "handler.h"

class InputHandlerProxy
{
	typedef std::multimap<u32, InputHandler*> InputHandlerStore;
	private:
		InputHandlerStore handlers;
	public:
		~InputHandlerProxy();
		void add(u32 port, InputHandler* handler);
		void handle(u32 port);
};