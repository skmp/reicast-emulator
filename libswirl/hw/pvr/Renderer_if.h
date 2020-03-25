/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

extern "C" {
	void rend_init_renderer(u8* vram);
	void rend_term_renderer();
	void rend_vblank();

	void rend_start_render(u8* vram);
	void rend_end_render();

	void rend_set_fb_scale(float x,float y);
	void rend_resize(int width, int height);
	void rend_text_invl(vram_block* bl);
}

///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct Renderer;

struct rendererbackend_t {
	string slug;
	string desc;
	int priority;
	Renderer* (*create)(u8* vram);
};

struct Renderer
{
	rendererbackend_t backendInfo;	// gets filled by renderer_if

	virtual bool Init()=0;
	
	virtual void Resize(int w, int h)=0;
	virtual void SetFBScale(float x, float y) = 0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool RenderPVR()=0;
    virtual bool RenderFramebuffer() = 0;
	virtual bool RenderLastFrame() { return false; }

	virtual void Present()=0;

	virtual u32 GetTexture(TSP tsp, TCW tcw) { return 0; }

	virtual ~Renderer() { }
};

extern bool renderer_enabled;	// Signals the renderer thread to exit
extern bool renderer_changed;	// Signals the renderer thread to switch renderer

extern bool RegisterRendererBackend(const rendererbackend_t& backend);
vector<rendererbackend_t> rend_get_backends();