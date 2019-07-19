#include "lua_bindings.h"

#include <string>
#include <vector>

#include "types.h"
#include "stdclass.h"

#ifdef SCRIPTING
// TODO: Allow more than one script
class LuaScript {
	lua_State* L;

	bool hasFrameFunc = true;
	bool hasStartFunc = true;
	bool hasStopFunc = true;
	bool hasResetFunc = true;

public:
	LuaScript(const char* fn)
	{
		// Create new Lua state and load the lua libraries
		L = luaL_newstate();
		luaL_openlibs(L);

		emulib_expose(L);

		luaL_dofile(L, fn);
	}

	~LuaScript()
	{
		lua_close(L);
	}

	void onframe()
	{
		if (!hasFrameFunc)
			return;
		lua_getglobal(L, "onFrame");
		if (lua_isfunction(L, -1) == 1) {
			if (lua_pcall(L, 0, 0, 0) != 0)
				printf("error running function `onFrame': %s\n", lua_tostring(L, -1));
		}
		else {
			lua_pop(L, 1);
			hasFrameFunc = false;
		}
	}

	void onstart()
	{
		if (!hasStartFunc)
			return;
		lua_getglobal(L, "onStart");
		if (lua_isfunction(L, -1) == 1) {
			if (lua_pcall(L, 0, 0, 0) != 0)
				printf("error running function `onStart': %s\n", lua_tostring(L, -1));
		}
		else {
			lua_pop(L, 1);
			hasStartFunc = false;
		}
	}

	void onstop()
	{
		if (!hasStopFunc)
			return;
		lua_getglobal(L, "onStop");
		if (lua_isfunction(L, -1) == 1) {
			if (lua_pcall(L, 0, 0, 0) != 0)
				printf("error running function `onStop': %s\n", lua_tostring(L, -1));
		}
		else {
			lua_pop(L, 1);
			hasStopFunc = false;
		}
	}

	void onreset()
	{
		if (!hasResetFunc)
			return;
		lua_getglobal(L, "onReset");
		if (lua_isfunction(L, -1) == 1) {
			if (lua_pcall(L, 0, 0, 0) != 0)
				printf("error running function `onReset': %s\n", lua_tostring(L, -1));
		}
		else {
			lua_pop(L, 1);
			hasResetFunc = false;
		}
	}
};

std::vector<LuaScript*> loaded_scripts;
#endif

void luabindings_findscripts(std::string path)
{
	std::string lua_path = path + "/main.lua";
	if (file_exists(lua_path))
	{
		luabindings_run(lua_path.c_str());
	}
}

void luabindings_run(const char* fn)
{
#ifdef SCRIPTING
	loaded_scripts.push_back(new LuaScript(fn));
#endif
}

void luabindings_close()
{
#ifdef SCRIPTING
	for (auto script : loaded_scripts)
	{
		delete script;
	}
	loaded_scripts.clear();
#endif
}

void luabindings_onframe()
{
#ifdef SCRIPTING
	for (auto script : loaded_scripts)
	{
		script->onframe();
	}
#endif
}

void luabindings_onstart()
{
#ifdef SCRIPTING
	for (auto script : loaded_scripts)
	{
		script->onstart();
	}
#endif
}

void luabindings_onstop()
{
#ifdef SCRIPTING
	for (auto script : loaded_scripts)
	{
		script->onstop();
	}
#endif
}

void luabindings_onreset()
{
#ifdef SCRIPTING
	for (auto script : loaded_scripts)
	{
		script->onreset();
	}
#endif
}