/*
 *	qt_osd.cpp
 *
*/
#include "types.h"
#include "oslib.h"

#include "qt_osd.h"



u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
	puts(text);
}

void os_DoEvents() {

}

void os_SetupInput() {
	//mcfg_CreateDevicesFromConfig();
}

void UpdateInputState(u32 port) {

}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() { }


void* libPvr_GetRenderTarget()  { return 0; }
void* libPvr_GetRenderSurface() { return 0; }

/*
bool gl_init(void*, void*) { return true; }
void gl_term() { }
void gl_swap() { }
*/
