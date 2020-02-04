/*
	This file is part of libswirl
*/
#include "license/bsd"



#include "hw/pvr/Renderer_if.h"
#include "oslib/oslib.h"

struct norend : Renderer
{
	bool Init()
	{
		return true;
	}
	void SetFBScale(float x, float y) { }
	void Resize(int w, int h) { }
	void Term() { }


    bool Process(TA_context* ctx) { return true; }

	bool RenderPVR()
	{
		return true;//!pvrrc.isRTT;
	}

    bool RenderFramebuffer()
    {
        return true;
    }

	void Present() { }
};

#include "hw/pvr/Renderer_if.h"

Renderer* rend_norend(u8* vram) { return new ::norend(); }

static auto norend = RegisterRendererBackend(rendererbackend_t{ "none", "No PVR Rendering", -2, rend_norend });