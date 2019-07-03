#pragma once

#include "lua/lua.hpp"

void emulib_expose(lua_State* L);

void luabindings_run(const char* fn);
void luabindings_close();
void luabindings_onframe();
void luabindings_onstart();
void luabindings_onstop();
void luabindings_onreset();
