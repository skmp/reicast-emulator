/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "lua/lua.hpp"
#include <string>

void emulib_expose(lua_State* L);

void luabindings_findscripts(std::string path);
void luabindings_run(const char* fn);
void luabindings_close();
void luabindings_onframe();
void luabindings_onstart();
void luabindings_onstop();
void luabindings_onreset();
