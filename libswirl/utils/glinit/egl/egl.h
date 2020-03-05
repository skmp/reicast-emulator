/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

bool egl_MakeCurrent();
void egl_GetCurrent();
bool egl_Init(void* wind, void* disp);
bool egl_Swap();
void egl_Term();