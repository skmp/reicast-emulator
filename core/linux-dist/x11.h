#pragma once

extern void* x11_glc;

#if defined(USE_EVDEV)
	#include "linux-dist/evdev.h"
	extern void input_x11_init(EvdevController *cntrl);
#else
	extern void input_x11_init();
#endif

extern void input_x11_handle();
extern void x11_window_create();
extern void x11_window_set_text(const char* text);
