#pragma once
#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

bool rend_init();
void rend_term();

void rend_vblank();
void rend_start_render();
void rend_end_render();
void rend_end_wait();
void rend_swap_frame();

void rend_set_fb_scale(float x,float y);
void rend_resize(int width, int height);

#ifdef GLuint
GLuint
#else
u32
#endif
GetTexture(TSP tsp,TCW tcw);


///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct Renderer
{
	virtual ~Renderer() = default;

	virtual bool Init()=0;
	
	virtual void Resize(int w, int h)=0;

	virtual void Term()=0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool Render()=0;

	virtual void Present()=0;

	virtual void DrawOSD() { }

	virtual u64 GetTexture(TSP tsp, TCW tcw) { return 0; }
};

extern Renderer* renderer;
extern bool renderer_changed;


Renderer* rend_D3D11();
Renderer* rend_GLES2();
Renderer* rend_GL4();
Renderer* rend_norend();
Renderer* rend_softrend();
Renderer* rend_Vulkan();
Renderer* rend_OITVulkan();

extern u32 fb_watch_addr_start;
extern u32 fb_watch_addr_end;
extern bool fb_dirty;

void check_framebuffer_write();

void rend_create_renderer();
void rend_init_renderer();
void rend_term_renderer();
