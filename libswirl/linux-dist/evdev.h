/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

extern void input_evdev_init();
extern void input_evdev_close();
extern bool input_evdev_handle(u32 port);
