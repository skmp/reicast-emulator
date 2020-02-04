/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include <SDL2/SDL.h>
#include "types.h"

extern void input_sdl_init();
extern void input_sdl_handle(u32 port);
extern bool sdl_window_create(void** wind, void** disp);
extern void sdl_window_set_text(const char* text);
