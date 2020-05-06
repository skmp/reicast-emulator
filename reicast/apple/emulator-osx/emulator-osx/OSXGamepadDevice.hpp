/*
    This file is part of reicast-osx
        original code by flyinghead
*/
#include "license/bsd"

#include "input/gamepad_device.h"
#include "input.h"
#include "gui/gui.h"

class KbInputMapping : public InputMapping {
public:
	KbInputMapping() {
		name = "OSX Keyboard";
		set_button(DC_BTN_A, kVK_ANSI_X);
		set_button(DC_BTN_B, kVK_ANSI_C);
		set_button(DC_BTN_X, kVK_ANSI_S);
		set_button(DC_BTN_Y, kVK_ANSI_D);
		set_button(DC_DPAD_UP, kVK_UpArrow);
		set_button(DC_DPAD_DOWN, kVK_DownArrow);
		set_button(DC_DPAD_LEFT, kVK_LeftArrow);
		set_button(DC_DPAD_RIGHT, kVK_RightArrow);
		set_button(DC_BTN_START, kVK_Return);
		set_button(EMU_BTN_TRIGGER_LEFT, kVK_ANSI_F);
		set_button(EMU_BTN_TRIGGER_RIGHT, kVK_ANSI_V);
		set_button(EMU_BTN_MENU, kVK_Tab);
		
		dirty = false;
	}
};

class OSXKbGamepadDevice : public GamepadDevice {
public:
	OSXKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "OSX") {
		_name = "Keyboard";
		_unique_id = "osx_keyboard";
		if (!find_mapping())
			input_mapper = new KbInputMapping();
	}
};

class MouseInputMapping : public InputMapping {
public:
	MouseInputMapping() {
		name = "OSX Mouse";
		set_button(DC_BTN_A, 1);		// Left button
		set_button(DC_BTN_B, 2);		// Right button
		set_button(DC_BTN_START, 3);	// Other button
        
		dirty = false;
	}
};

class OSXMouseGamepadDevice : public GamepadDevice {
public:
	OSXMouseGamepadDevice(int maple_port) : GamepadDevice(maple_port, "OSX") {
		_name = "Mouse";
		_unique_id = "osx_mouse";
		if (!find_mapping())
			input_mapper = new MouseInputMapping();
	}
	
    bool gamepad_btn_input(u32 code, bool pressed) override {
		if (g_GUI->IsOpen())
			// Don't register mouse clicks as gamepad presses when gui is open
			// This makes the gamepad presses to be handled first and the mouse position to be ignored
			// TODO Make this generic
			return false;
		else
			return GamepadDevice::gamepad_btn_input(code, pressed);
	}
};

enum OSXGameControllerKeys : u32 {
    OSX_BTN_A, OSX_BTN_B, OSX_BTN_X, OSX_BTN_Y,
    OSX_BTN_START, OSX_BTN_SELECT,
    OSX_DPAD_UP, OSX_DPAD_DOWN, OSX_DPAD_LEFT, OSX_DPAD_RIGHT,
    OSX_BTN_LB, OSX_BTN_RB,
    OSX_AXIS_LT, OSX_AXIS_RT,
    OSX_AXIS_LS_X, OSX_AXIS_LS_Y,
    OSX_AXIS_RS_X, OSX_AXIS_RS_Y
};

class GameControllerInputMapping : public InputMapping {
public:
    GameControllerInputMapping() {
        name = "OSX Game Controller";
        set_button(DC_BTN_A, OSX_BTN_A);
        set_button(DC_BTN_B, OSX_BTN_B);
        set_button(DC_BTN_X, OSX_BTN_X);
        set_button(DC_BTN_Y, OSX_BTN_Y);
        set_button(DC_DPAD_UP, OSX_DPAD_UP);
        set_button(DC_DPAD_DOWN, OSX_DPAD_DOWN);
        set_button(DC_DPAD_LEFT, OSX_DPAD_LEFT);
        set_button(DC_DPAD_RIGHT, OSX_DPAD_RIGHT);
        set_button(DC_BTN_START, OSX_BTN_START);
        set_button(EMU_BTN_MENU, OSX_BTN_SELECT);
        
        set_axis(DC_AXIS_LT, OSX_AXIS_LT, false);
        set_axis(DC_AXIS_RT, OSX_AXIS_RT, false);
        set_axis(DC_AXIS_X, OSX_AXIS_LS_X, false);
        set_axis(DC_AXIS_Y, OSX_AXIS_LS_Y, false);
        
        set_axis(DC_AXIS_X2, OSX_AXIS_RS_X, false);
        set_axis(DC_AXIS_Y2, OSX_AXIS_RS_Y, false);
        
        dirty = false;
    }
    
};
