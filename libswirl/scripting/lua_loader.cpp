#include "lua_bindings.h"

#include <string>
#include <vector>

#include "types.h"
#include "stdclass.h"

#include "mods.h"

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

static void luabindings_run(const char* fn)
{
	loaded_scripts.push_back(new LuaScript(fn));
}

static bool luabindings_findscripts(std::string path)
{
	// TODO: config option to disable LUA loading

	std::string lua_path = path + "/main.lua";
	if (file_exists(lua_path))
	{
		luabindings_run(lua_path.c_str());
		return true;
	}
	return false;
}

static void luabindings_close()
{
	for (auto& script : loaded_scripts)
	{
		delete script;
	}
	loaded_scripts.clear();
}

static void luabindings_onframe()
{
	for (auto& script : loaded_scripts)
	{
		script->onframe();
	}
}

static void luabindings_onstart()
{
	for (auto& script : loaded_scripts)
	{
		script->onstart();
	}
}

static void luabindings_onstop()
{
	for (auto& script : loaded_scripts)
	{
		script->onstop();
	}
}

static void luabindings_onreset()
{
	for (auto& script : loaded_scripts)
	{
		script->onreset();
	}
}

static mod_handlers luamod = {
	luabindings_findscripts,
	luabindings_close,
	luabindings_onframe,
	luabindings_onstart,
	luabindings_onstop,
	luabindings_onreset
};
#ifdef SCRIPTING
static bool registered = mod_handler_register(luamod);
#endif