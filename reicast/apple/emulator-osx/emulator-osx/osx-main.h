//
//  osx-main.h
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#ifndef emulator_osx_osx_main_h
#define emulator_osx_osx_main_h
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void emu_dc_exit();
int emu_single_frame(int w, int h);
void emu_gles_init(int width, int height);
int emu_reicast_init();
void emu_start_ui_loop();
void emu_key_input(UInt16 keyCode, bool pressed, UInt32 modifierFlags);
void emu_character_input(const char *characters);
void emu_mouse_buttons(int button, bool pressed);

bool emu_frame_pending();
extern unsigned int mo_buttons;
extern int mo_x_abs;
extern int mo_y_abs;
extern float mo_x_delta;
extern float mo_y_delta;
extern float mo_wheel_delta;

#ifdef __cplusplus
}
#endif

#endif
