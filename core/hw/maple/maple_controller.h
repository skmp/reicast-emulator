#pragma once
#include "types.h"

// If you change the value of MAPLE_NUM_PORTS, please note that you need to change the initializers in maple_controller.cpp as well
#define MAPLE_NUM_PORTS 4

extern u16 kcode[MAPLE_NUM_PORTS];
extern u32 vks[MAPLE_NUM_PORTS];
extern u8 rt[MAPLE_NUM_PORTS];
extern u8 lt[MAPLE_NUM_PORTS];
extern s8 joyx[MAPLE_NUM_PORTS];
extern s8 joyy[MAPLE_NUM_PORTS];
extern bool port_enabled[MAPLE_NUM_PORTS];

enum MapleControllerCode
{
	DC_BTN_C           = 1,
	DC_BTN_B           = 1<<1,
	DC_BTN_A           = 1<<2,
	DC_BTN_START       = 1<<3,
	DC_BTN_DPAD_UP     = 1<<4,
	DC_BTN_DPAD_DOWN   = 1<<5,
	DC_BTN_DPAD_LEFT   = 1<<6,
	DC_BTN_DPAD_RIGHT  = 1<<7,
	DC_BTN_Z           = 1<<8,
	DC_BTN_Y           = 1<<9,
	DC_BTN_X           = 1<<10,
	DC_BTN_D           = 1<<11,
	DC_BTN_DPAD2_UP    = 1<<12,
	DC_BTN_DPAD2_DOWN  = 1<<13,
	DC_BTN_DPAD2_LEFT  = 1<<14,
	DC_BTN_DPAD2_RIGHT = 1<<15,

	DC_AXIS_LT = 0X10000,
	DC_AXIS_RT = 0X10001,
	DC_AXIS_X  = 0X20000,
	DC_AXIS_Y  = 0X20001
};
