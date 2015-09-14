#pragma once
#include "types.h"

extern u16 kcode[4];
extern u32 vks[4];
extern u8 rt[4], lt[4];
extern s8 joyx[4], joyy[4];

extern void* x11_win;
extern void* x11_disp;

enum DreamcastController
{
	EMU_BTN_NONE   = 0,
	EMU_BTN_ESCAPE = 1<<16,
	EMU_BTN_TRIGGER_LEFT  = 1<<17,
	EMU_BTN_TRIGGER_RIGHT = 1<<18,
	DC_BTN_C           = 1,
	DC_BTN_B           = 1<<1,
	DC_BTN_A           = 1<<2,
	DC_BTN_START       = 1<<3,
	DC_BTN_DPAD1_UP    = 1<<4,
	DC_BTN_DPAD1_DOWN  = 1<<5,
	DC_BTN_DPAD1_LEFT  = 1<<6,
	DC_BTN_DPAD1_RIGHT = 1<<7,
	DC_BTN_Z           = 1<<8,
	DC_BTN_Y           = 1<<9,
	DC_BTN_X           = 1<<10,
	DC_BTN_D           = 1<<11,
	DC_BTN_DPAD2_UP    = 1<<12,
	DC_BTN_DPAD2_DOWN  = 1<<13,
	DC_BTN_DPAD2_LEFT  = 1<<14,
	DC_BTN_DPAD2_RIGHT = 1<<15,

	EMU_AXIS_NONE    = 0X00000,
	EMU_AXIS_DPAD1_X = 0X00001,
	EMU_AXIS_DPAD1_Y = 0X00002,
	EMU_AXIS_DPAD2_X = 0X00003,
	EMU_AXIS_DPAD2_Y = 0X00004,
	DC_AXIS_TRIGGER_LEFT = 0X10000,
	DC_AXIS_TRIGGER_RIGHT = 0X10001,
	DC_AXIS_X        = 0X20000,
	DC_AXIS_Y        = 0X20001,
};
