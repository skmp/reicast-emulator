#pragma once
#include "drkPvr.h"
#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

bool rend_init(void);
void rend_term(void);

void rend_vblank(void);
void rend_start_render(void);
void rend_end_render(void);
void rend_end_wait(void);

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

	virtual void DrawOSD() { }

	virtual u32 GetTexture(TSP tsp, TCW tcw) { return 0; }
};

extern Renderer* renderer;


Renderer* rend_GLES2();
Renderer* rend_softrend();
