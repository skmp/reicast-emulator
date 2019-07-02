#pragma once
#include "drkPvr.h"
#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

enum eRenderer
{
	eRenderer_CompatOffset = 9,
	R_None,
	R_GLES,
	R_GL4,
	R_D3D11,
	R_Soft,

	R_Vulkan,
	R_GNM,

	eRenderer_Count,

	R_Auto = -1,
};


//#if defined(GLES) || defined(GL4)
//typedef GLuint TexID;
//#else
typedef u32 TexID;
//#endif

TexID GetTexture(TSP tsp, TCW tcw);


///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)







extern u32 fb1_watch_addr_start;
extern u32 fb1_watch_addr_end;
extern u32 fb2_watch_addr_start;
extern u32 fb2_watch_addr_end;
extern bool fb_dirty;

void check_framebuffer_write();



void rend_init_renderer(eRenderer rID = R_Auto);
void rend_term_renderer();
void rend_stop_renderer();
void rend_vblank();
void rend_start_render();
void rend_end_render();
void rend_cancel_emu_wait();
bool rend_single_frame();

//void rend_set_fb_scale(float x, float y);
void rend_resize(int width, int height);
//void rend_text_invl(vram_block* bl);

#include <functional>

class Renderer
{
	typedef std::function<Renderer* (void)> CreateFn;

	static map<eRenderer, CreateFn> m_map;
	static Renderer * m_renderer, * m_fallback;

public:
	static inline void Reset() {
		m_renderer = m_fallback = nullptr;
	}
	static inline Renderer* Get() {
		return m_renderer;
	}
	static inline void Set(Renderer* rPtr) {
		if (rPtr) m_renderer = rPtr;
	}
	static inline Renderer* Fallback() {
		return m_fallback;
	}
	static inline void SetFallback(Renderer* rPtr) {
		if (rPtr) m_fallback = rPtr;
	}

	static bool Register(const eRenderer rID, CreateFn createFn)
	{
		if (m_map.count(rID))
			return false;

		m_map[rID] = createFn;
		return true;
	}


	static bool Create(const eRenderer rID, bool asFallback = false)
	{
	//	printf("Renderer::Create(%X,%s) \n", (u32)rID, asFallback ? "true" : "false");
	//	printf("map has %d entries:\n", m_map.size());
	//	for (auto it = m_map.begin(); it != m_map.end(); ++it)
	//		printf("-entry: %X := %p \n", it->first , it->second );

		Renderer* r = nullptr;
		CreateFn createFn = m_map.at(rID);

		if (createFn && (r = createFn())) {
			if (!asFallback) Set(r);
			else SetFallback(r);
		}
		return nullptr != r;
	}

	static void Destroy() {
		if (m_renderer) delete m_renderer;
		if (m_fallback) delete m_fallback;
		Reset();
	}


// The interface :

//	static Renderer* createInstance();


	virtual bool Init()=0;
	
	virtual void Resize(int w, int h)=0;

	virtual void Term()=0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool Render()=0;
	virtual bool RenderLastFrame() { return false; }

	virtual void Present()=0;

	virtual void DrawOSD(bool clear_screen) { }

	virtual void Invalidate(vram_block* bl) = 0;

	virtual TexID GetTexture(TSP tsp, TCW tcw) { return 0; }
};

#define renderer Renderer::Get()
#define fallback_renderer Renderer::Fallback()

//extern Renderer* renderer;
extern bool renderer_enabled;	// Signals the renderer thread to exit
extern bool renderer_changed;	// Signals the renderer thread to switch renderer



Renderer* rend_norend();

#if defined(GLES) || defined(USE_GLES)
Renderer* rend_GLES2();
#endif

#if defined(GL4) || defined(USE_GL4) || !defined(GLES) && HOST_OS != OS_DARWIN
Renderer* rend_GL4();
#endif

#ifdef USE_SOFT
Renderer* rend_softrend();
#endif

#ifdef USE_VULKAN
Renderer* vk_renderer();
#endif

