/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "lua_bindings.h"
#include "hw/pvr/Renderer_if.h"
#include "gui/gui.h"
#include "hw/sh4/sh4_mem.h"
#include "libswirl.h"

extern u32 vblank_count_monotonic;

static int emu_status(lua_State* L) {
	if (g_GUI->IsOpen())
		lua_pushliteral(L, "menu");
	else
		lua_pushliteral(L, "ingame");
	return 1;
}

extern bool game_started;

static int emu_start_game(lua_State* L) {
	int n = lua_gettop(L);  /* number of arguments */
	luaL_argcheck(L, n == 1, 1, "A single path argument expected");
	luaL_checktype(L, 1, LUA_TSTRING);

	const char* str = lua_tostring(L, 1);

	int ret = virtualDreamcast->StartGame(str);
    game_started = ret -= 0;
	lua_pushinteger(L, ret);

	return 1;
}

static int emu_resume(lua_State* L) {
	// TODO: verify it's not already started
    virtualDreamcast->Resume();
	return 0;
}

static int emu_stop(lua_State* L) {
	// TODO: verify it's not already stopped
    //virtualDreamcast->Stop();
	die("stop is async now. what to do in lua?");
	return 0;
}

static int emu_reset(lua_State* L) {
    virtualDreamcast->Reset();
	return 0;
}

static int emu_next_frame(lua_State* L) {
	// TODO
	return 0;
}

static int emu_get_vblank_count(lua_State* L) {
	lua_pushinteger(L, vblank_count_monotonic);
	return 1;
}

static int emu_change_disk(lua_State* L) {
	// TODO
	return 0;
}

static int emu_save_state(lua_State* L) {
    virtualDreamcast->SaveState();
	return 1;
}

static int emu_load_state(lua_State* L) {
    virtualDreamcast->LoadState();
	return 1;
}

static int emu_get_inputs(lua_State* L) {
	// TODO
	return 1;
}

template<class T>
inline static T _read_main(u32 addr) {
	if (sizeof(T) == 1) return (T)ReadMem8(addr);
	else if (sizeof(T) == 2) return (T)ReadMem16(addr);
	else if (sizeof(T) == 4) return (T)ReadMem32(addr);
	else return (T)ReadMem64(addr);
}

template<class T>
inline static void _write_main(u32 addr, T value) {
	if (sizeof(T) == 1) WriteMem8(addr, (u8)value);
	else if (sizeof(T) == 2) WriteMem16(addr, (u16)value);
	else if (sizeof(T) == 4) WriteMem32(addr, (u32)value);
	else WriteMem64(addr, (u64)value);
}

template<class T>
inline static T _read_sound(u32 addr) {
	return (T)0;
}

template<class T>
inline static void _write_sound(u32 addr, T value) {

}

template<class T>
static int emu_read_main(lua_State* L) {
	int n = lua_gettop(L);
	luaL_argcheck(L, n >= 1 && n <= 2, 1, "An address argument and an optional count argument expected");
	luaL_checkinteger(L, 1);
	if (n==2)
		luaL_checkinteger(L, 1);
	u32 addr = (u32)lua_tointeger(L, 1);
	if (n == 1) {
		T mem = _read_main<T>(addr);
		lua_pushinteger(L, mem);
	}
	else
	{
		size_t num_results = (size_t)lua_tointeger(L, 2);
		lua_createtable(L, (int)num_results, 0);
		for (size_t i = 0; i < num_results; i++) {
			T mem = _read_main<T>(addr);
			lua_pushinteger(L, mem);
			lua_rawseti(L, -2, (lua_Integer)i + 1); /* In lua indices start at 1 */
			addr += sizeof(T);
		}
	}
	return 1;
}

template int emu_read_main<s8>(lua_State* L);
template int emu_read_main<s16>(lua_State* L);
template int emu_read_main<s32>(lua_State* L);
template int emu_read_main<s64>(lua_State* L);
template int emu_read_main<u8>(lua_State* L);
template int emu_read_main<u16>(lua_State* L);
template int emu_read_main<u32>(lua_State* L);
template int emu_read_main<u64>(lua_State* L);

template<class T>
static int emu_write_main(lua_State* L) {
	int n = lua_gettop(L);
	luaL_argcheck(L, n >= 2, 1, "Address and value/array expected");
	luaL_checkinteger(L, 1);
	u32 addr = (u32)lua_tointeger(L, 1);
	if (lua_isinteger(L, 2))
	{
		T value = (T)lua_tointeger(L, 2);
		_write_main<T>(addr, value);
	}
	else if (lua_istable(L, 2))
	{
		size_t num_results = lua_rawlen(L, 2);
		for (size_t i = 0; i < num_results; i++) {
			T value = (T)lua_rawgeti(L, 2, (lua_Integer)i + 1); /* In lua indices start at 1 */
			_write_main<T>(addr, value);
			addr += sizeof(T);
		}
	}
	else
	{
		luaL_argerror(L, 2, "expected integer or table");
	}
	return 0;
}

template int emu_write_main<s8>(lua_State* L);
template int emu_write_main<s16>(lua_State* L);
template int emu_write_main<s32>(lua_State* L);
template int emu_write_main<s64>(lua_State* L);

template<class T>
static int emu_read_sound(lua_State* L) {
	int n = lua_gettop(L);
	luaL_argcheck(L, n >= 1 && n <= 2, 1, "An address argument and an optional count argument expected");
	luaL_checkinteger(L, 1);
	if (n == 2)
		luaL_checkinteger(L, 1);
	u32 addr = (u32)lua_tointeger(L, 1);
	if (n == 1) {
		T mem = _read_sound<T>(addr);
		lua_pushinteger(L, mem);
	}
	else
	{
		size_t num_results = (size_t)lua_tointeger(L, 2);
		lua_createtable(L, (int)num_results, 0);
		for (size_t i = 0; i < num_results; i++) {
			T mem = _read_sound<T>(addr);
			lua_pushinteger(L, mem);
			lua_rawseti(L, -2, (lua_Integer)i + 1); /* In lua indices start at 1 */
			addr += sizeof(T);
		}
	}
	return 1;
}

template int emu_read_sound<s8>(lua_State* L);
template int emu_read_sound<s16>(lua_State* L);
template int emu_read_sound<s32>(lua_State* L);
template int emu_read_sound<u8>(lua_State* L);
template int emu_read_sound<u16>(lua_State* L);
template int emu_read_sound<u32>(lua_State* L);

template<class T>
static int emu_write_sound(lua_State* L) {
	int n = lua_gettop(L);
	luaL_argcheck(L, n >= 2, 1, "Address and value/array expected");
	luaL_checkinteger(L, 1);
	u32 addr = (u32)lua_tointeger(L, 1);
	if (lua_isinteger(L, 2))
	{
		T value = (T)lua_tointeger(L, 2);
		_write_sound<T>(addr, value);
	}
	else if (lua_istable(L, 2))
	{
		size_t num_results = lua_rawlen(L, 2);
		for (size_t i = 0; i < num_results; i++) {
			T value = (T)lua_rawgeti(L, 2, (lua_Integer)i + 1); /* In lua indices start at 1 */
			_write_sound<T>(addr, value);
			addr += sizeof(T);
		}
	}
	else
	{
		luaL_argerror(L, 2, "expected integer or table");
	}
	return 0;
}

template int emu_write_sound<s8>(lua_State* L);
template int emu_write_sound<s16>(lua_State* L);
template int emu_write_sound<s32>(lua_State* L);

static const luaL_Reg emulib[] = {

	// Emulation control
	{"status", emu_status},				// Queries emulation status
	{"start_game", emu_start_game},		// Starts a game (takes a path)
	{"resume", emu_resume},				// Resumes normal emulation
	{"stop", emu_stop},					// Pauses emulation (without losing context)
	{"reset", emu_reset},				// Resets the hardware
	{"advanceFrame", emu_next_frame},	// Emulates one frame (or vsync maybe)
	{"getVblankCount", emu_get_vblank_count},	// Gets the number of frames drawn since emulation start (probably should be a vsync count instead)
	{"changeDisk", emu_change_disk },	// changes the current disk image or 

	{"saveState", emu_save_state},		// Creates a savestate file
	{"loadState", emu_load_state},		// Loads state from a file

	{"getInputs", emu_get_inputs},		// Gets an object with the input state for the specified controller port

	// main cpu access
	{"readMemS8",  emu_read_main<s8> },
	{"readMemS16", emu_read_main<s16> },
	{"readMemS32", emu_read_main<s32> },
	{"readMemS64", emu_read_main<s64> },
	{"readMemU8",  emu_read_main<u8> },
	{"readMemU16", emu_read_main<u16> },
	{"readMemU32", emu_read_main<u32> },
	{"readMemU64", emu_read_main<u64> },
	{"writeMem8",  emu_write_main<s8> },
	{"writeMem16", emu_write_main<s16> },
	{"writeMem32", emu_write_main<s32> },
	{"writeMem64", emu_write_main<s64> },

	// sound cpu access
	{"readSoundMemS8",  emu_read_sound<s8> },
	{"readSoundMemS16", emu_read_sound<s16> },
	{"readSoundMemS32", emu_read_sound<s32> },
	{"readSoundMemU8",  emu_read_sound<u8> },
	{"readSoundMemU16", emu_read_sound<u16> },
	{"readSoundMemU32", emu_read_sound<u32> },
	{"writeSoundMemS8",  emu_write_sound<s8> },
	{"writeSoundMemS16", emu_write_sound<s16> },
	{"writeSoundMemS32", emu_write_sound<s32> },

	// ....MOAR!

	{NULL, NULL}
};


/*
** Open library
*/
LUAMOD_API int luaopen_emulib(lua_State* L) {
	luaL_newlib(L, emulib);
	return 1;
}

void emulib_expose(lua_State* L) {
	luaL_requiref(L, "reicast", luaopen_emulib, 1);
	lua_pop(L, 1);  /* remove lib */
}
