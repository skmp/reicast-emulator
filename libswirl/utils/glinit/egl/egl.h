#pragma once

bool egl_MakeCurrent();
void egl_GetCurrent();
bool egl_Init(void* wind, void* disp);
void egl_Swap();
void egl_Term();