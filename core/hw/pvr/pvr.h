#pragma once
#include "types.h"


//bleh stupid windoze header
#include "types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define fverify verify

#include "pvr_mem.h"
#include "pvr_regs.h"
#include "pvr_types.h"

#include "ta.h"

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
void rend_vblank(void);

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
