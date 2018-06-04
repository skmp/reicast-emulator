#pragma once
#include "types.h"


//bleh stupid windoze header
#include "types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define fverify verify

#include "pvr_regs.h"
#include "pvr_types.h"

#include "ta.h"

u32 pvr_map32(u32 offset32);

//vram 32-64b
extern VArray2 vram;
//read
u8 DYNACALL pvr_read_area1_8(u32 addr);
u16 DYNACALL pvr_read_area1_16(u32 addr);
u32 DYNACALL pvr_read_area1_32(u32 addr);
//write
void DYNACALL pvr_write_area1_8(u32 addr,u8 data);
void DYNACALL pvr_write_area1_16(u32 addr,u16 data);
void DYNACALL pvr_write_area1_32(u32 addr,u32 data);

//regs
void pvr_WriteReg(u32 paddr,u32 data);

void pvr_Update(u32 cycles);

//Init/Term , global
void pvr_Init(void);
void pvr_Term(void);
//Reset -> Reset - Initialise
void pvr_Reset(bool Manual);

void TAWrite(u32 address,u32* data,u32 count);
extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb);

//registers 
#define PVR_BASE 0x005F8000

void libCore_vramlock_Free(void);
void libCore_vramlock_Init(void);

//Init/Term , global
void pvr_sb_Init();
void pvr_sb_Term();
//Reset -> Reset - Initialise
void pvr_sb_Reset(bool Manual);

extern u32 VertexCount;
extern u32 FrameCount;

bool rend_init(void);
void rend_term(void);

void rend_end_render(void);

void rend_set_fb_scale(float x,float y);
void rend_resize(int width, int height);
void rend_text_invl(vram_block* bl);

#ifdef GLuint
GLuint
#else
u32
#endif
GetTexture(TSP tsp,TCW tcw);


extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct Renderer
{
	virtual bool Init()=0;
	
	virtual void Resize(int w, int h)=0;

	virtual void Term()=0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool Render()=0;

	virtual void Present()=0;

	virtual u32 GetTexture(TSP tsp, TCW tcw) { return 0; }
};

extern Renderer* renderer;


Renderer* rend_GLES2();
Renderer* rend_norend();
Renderer* rend_softrend();
