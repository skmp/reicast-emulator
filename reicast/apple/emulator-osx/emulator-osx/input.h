/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
@class GCExtendedGamepad;

void emu_key_input(UInt16 keyCode, bool pressed, UInt32 modifierFlags);
void emu_character_input(const char *characters);
void emu_mouse_buttons(int button, bool pressed);
void connect_controller(GCExtendedGamepad *controller);
void disconnect_controller(GCExtendedGamepad *controller);

extern unsigned int mo_buttons;
extern int mo_x_abs;
extern int mo_y_abs;
extern float mo_x_delta;
extern float mo_y_delta;
extern float mo_wheel_delta;

#ifdef __cplusplus
}
#endif
