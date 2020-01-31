#pragma once

void gui_settings();
void gui_settings_general();
void gui_settings_controls();
void gui_settings_video();
void gui_settings_audio();
void gui_settings_advanced();
void gui_settings_social();
void gui_settings_about();

void gui_welcome(ImFont* font64);

void reset_vmus();

extern bool game_list_done;
extern bool maple_devices_changed;
extern float scaling;
extern ImVec2 normal_padding;
extern int dynarec_enabled;
extern bool settings_opening;
extern bool game_started;
extern bool renderer_changed;


//TODO: MOVE TO PROPER FILES
extern int screen_width, screen_height;

// ALSO THESE
extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u8 kb_key[6];		// normal keys pressed
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;

//


/// ALSO MOVE THESE ////

extern int screen_dpi;

typedef enum { Welcome, Closed, Commands, Settings, Main, Onboarding, VJoyEdit, VJoyEditCommands } GuiState;
extern GuiState gui_state;
void ImGui_Impl_NewFrame();
