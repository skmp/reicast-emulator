/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#import <sys/stat.h>

#import "libswirl.h"
#import "types.h"
#import "hw/maple/maple_cfg.h"
#import "gui/gui.h"
#import "OSXKeyboardDevice.hpp"
#import "OSXGamepadDevice.hpp"

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);

u16 kcode[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

void os_SetupInput() {
	kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

extern "C" void emu_key_input(UInt16 keyCode, bool pressed, UInt modifierFlags) {
	keyboard.keyboard_input(keyCode, pressed, keyboard.convert_modifier_keys(modifierFlags));
	if ((modifierFlags
		 & (NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption | NSEventModifierFlagCommand)) == 0)
		kb_gamepad->gamepad_btn_input(keyCode, pressed);
}

extern "C" void emu_character_input(const char *characters) {
	if (characters != NULL)
		while (*characters != '\0')
			keyboard.keyboard_character(*characters++);
}

extern "C" void emu_mouse_buttons(int button, bool pressed)
{
	mouse_gamepad->gamepad_btn_input(button, pressed);
}
