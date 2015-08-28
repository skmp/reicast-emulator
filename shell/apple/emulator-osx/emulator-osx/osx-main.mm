//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>

#include "types.h"
#include "hw/maple/maple_controller.h"
#include <sys/stat.h>

#include <OpenGL/gl3.h>

int msgboxf(const wchar* text,unsigned int type,...)
{
    va_list args;
    
    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);
    
    puts(temp);
    return 0;
}

int darw_printf(const wchar* text,...) {
    va_list args;
    
    wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);
    
    NSLog(@"%s", temp);
    
    return 0;
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {
    
}


void UpdateInputState(u32 port) {
    
}

void os_CreateWindow() {
    
}

void* libPvr_GetRenderTarget() {
    return 0;
}

void* libPvr_GetRenderSurface() {
    return 0;
    
}

bool gl_init(void*, void*) {
    return true;
}

void gl_term() {
    
}

void gl_swap() {
    
}

int dc_init(int argc,wchar* argv[]);
void dc_run();

bool has_init = false;
void* emuthread(void*) {
    settings.profile.run_counts=0;
    string home = (string)getenv("HOME");
    if(home.c_str())
    {
        home += "/.reicast";
        mkdir(home.c_str(), 0755); // create the directory if missing
        set_user_config_dir(home);
        set_user_data_dir(home);
    }
    else
    {
        set_user_config_dir(".");
        set_user_data_dir(".");
    }
    char* argv[] = { "reicast" };
    
    dc_init(1,argv);
    
    has_init = true;
    
    dc_run();
    
    return 0;
}

pthread_t emu_thread;
extern "C" void emu_main() {
    pthread_create(&emu_thread, 0, &emuthread, 0);
}

extern int screen_width,screen_height;
bool rend_single_frame();
bool gles_init();

extern "C" bool emu_single_frame(int w, int h) {
    if (!has_init)
        return true;
    screen_width = w;
    screen_height = h;
    return rend_single_frame();
}

extern "C" void emu_gles_init() {
    gles_init();
}

void handle_key(int dckey, int state) {
    if (state)
        maple_controller[0].buttons &= ~dckey;
    else
        maple_controller[0].buttons |= dckey;
}

void handle_trig(u8* dckey, int state) {
    if (state)
        dckey[0] = 255;
    else
        dckey[0] = 0;
}

extern "C" void emu_key_input(char* keyt, int state) {
    int key = keyt[0];
    switch(key) {
        case 'z':     handle_key(DC_BTN_X, state); break;
        case 'x':     handle_key(DC_BTN_Y, state); break;
        case 'c':     handle_key(DC_BTN_B, state); break;
        case 'v':     handle_key(DC_BTN_A, state); break;
        
        case 'a':     handle_trig(lt, state); break;
        case 's':     handle_trig(rt, state); break;
            
        case 'j':     handle_key(DC_BTN_DPAD_LEFT,  state); break;
        case 'k':     handle_key(DC_BTN_DPAD_DOWN,  state); break;
        case 'l':     handle_key(DC_BTN_DPAD_RIGHT, state); break;
        case 'i':     handle_key(DC_BTN_DPAD_UP,    state); break;
        case 0xa:     handle_key(DC_BTN_START,      state); break;
    }
}