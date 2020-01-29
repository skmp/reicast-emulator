#pragma once
#include <GL/glx.h>

bool glx_ChooseVisual(Display* display, long screen, XVisualInfo** visual, int* depth);
bool glx_Init(void* wind, void* disp);
bool glx_Swap();
void glx_Term();