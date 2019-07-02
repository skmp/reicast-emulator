
#include "hw/pvr/Renderer_if.h"
#include "oslib/oslib.h"

//void rend_set_fb_scale(float x,float y) { }
//void rend_text_invl(vram_block* bl) { }

struct norend
	: Renderer
{

	bool Init()
	{
		return true;
	}

	void Resize(int w, int h) { }
	void Term() { }


        bool Process(TA_context* ctx) { return true; }

        void DrawOSD(bool clear_screen) {  }

	bool Render()
	{
		return true;//!pvrrc.isRTT;
	}

	void Present() { }

	void Invalidate(vram_block* bl)
	{	}

	TexID GetTexture(TSP tsp, TCW tcw) { return 0; }

	static Renderer* createInstance() {
		return new norend;
	}
};

#if 0
Renderer* rend_norend() { return new norend(); }
#else

static bool rr_norend = Renderer::Register(R_None, &norend::createInstance);
#endif


