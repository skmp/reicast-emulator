#pragma once

#include <string>
#include <vector>
#include <stdclass.h>

struct mod_handlers {
	// required
	bool (*detect)(std::string);
	void (*close)();

	// optional
	void (*onframe)();
	void (*onstart)();
	void (*onstop)();
	void (*onreset)();

	bool active;
};
bool mod_handler_register(mod_handlers& mod);

void modpack_gamestart();
void modpack_close();
void modpack_onframe();
void modpack_onstart();
void modpack_onstop();
void modpack_onreset();

std::string get_game_id();