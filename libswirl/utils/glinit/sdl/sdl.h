/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

bool sdlgl_CreateWindow(bool gles, int flags, int window_width, int window_height, void** wind, void** disp);
bool sdlgl_Init(void* wind, void* disp);
bool sdlgl_Swap();
void sdlgl_Term();