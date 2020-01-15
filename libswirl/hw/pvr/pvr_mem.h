#pragma once
#include "types.h"
#include "types.h"



u32 pvr_map32(u32 offset32);
f32 vrf(u8* vram, u32 addr);
u32 vri(u8* vram, u32 addr);

//vram 32-64b


extern bool fb_dirty;

void pvr_update_framebuffer_watches();

//read
u8 DYNACALL pvr_read_area1_8(SuperH4*, u32 addr);
u16 DYNACALL pvr_read_area1_16(SuperH4*, u32 addr);
u32 DYNACALL pvr_read_area1_32(SuperH4*, u32 addr);
//write
void DYNACALL pvr_write_area1_8(SuperH4*, u32 addr,u8 data);
void DYNACALL pvr_write_area1_16(SuperH4*, u32 addr,u16 data);
void DYNACALL pvr_write_area1_32(SuperH4*, u32 addr,u32 data);

//regs

void pvr_Update(u32 cycles);

//Init/Term , global
void pvr_Init();
void pvr_Term();
//Reset -> Reset - Initialise
void pvr_Reset(bool Manual);

void TAWrite(u32 address,u32* data,u32 count, u8* vram);
extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb);

void YUV_init();
//registers 
#define PVR_BASE 0x005F8000
