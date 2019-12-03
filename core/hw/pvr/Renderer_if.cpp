#include "Renderer_if.h"
#include "ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"
#include "cheats.h"

/*

	rendv3 ideas
	- multiple backends
	  - ESish
	    - OpenGL ES2.0
	    - OpenGL ES3.0
	    - OpenGL 3.1
	  - OpenGL 4.x
	  - Direct3D 10+ ?
	- correct memory ordering model
	- resource pools
	- threaded TA
	- threaded rendering
	- RTTs
	- framebuffers
	- overlays


	PHASES
	- TA submission (memops, dma)

	- TA parsing (defered, rend thread)

	- CORE render (in-order, defered, rend thread)


	submission is done in-order
	- Partial handling of TA values
	- Gotchas with TA contexts

	parsing is done on demand and out-of-order, and might be skipped
	- output is only consumed by renderer

	render is queued on RENDER_START, and won't stall the emulation or might be skipped
	- VRAM integrity is an issue with out-of-order or delayed rendering.
	- selective vram snapshots require TA parsing to complete in order with REND_START / REND_END


	Complications
	- For some apis (gles2, maybe gl31) texture allocation needs to happen on the gpu thread
	- multiple versions of different time snapshots of the same texture are required
	- TA parsing vs frameskip logic


	Texture versioning and staging
	 A memory copy of the texture can be used to temporary store the texture before upload to vram
	 This can be moved to another thread
	 If the api supports async resource creation, we don't need the extra copy
	 Texcache lookups need to be versioned


	rendv2x hacks
	- Only a single pending render. Any renders while still pending are dropped (before parsing)
	- wait and block for parse/texcache. Render is async
*/

extern int screen_width;
extern int screen_height;

u32 VertexCount=0;
u32 FrameCount=1;

Renderer* renderer;
static Renderer* fallback_renderer;
bool renderer_changed = false;	// Signals the renderer interface to switch renderer

#if !defined(TARGET_NO_THREADS)
cResetEvent rs;
cResetEvent re;
#endif
extern cResetEvent frame_finished;
static bool swap_pending;
static bool do_swap;

int max_idx,max_mvo,max_op,max_pt,max_tr,max_vtx,max_modt, ovrn;
bool pend_rend = false;

static bool render_called = false;
u32 fb_watch_addr_start;
u32 fb_watch_addr_end;
bool fb_dirty;

TA_context* _pvrrc;
void SetREP(TA_context* cntx);

void rend_create_renderer()
{
#ifdef NO_REND
	renderer	 = rend_norend();
#else
	switch (settings.pvr.rend)
	{
	default:
	case 0:
		NOTICE_LOG(PVR, "Creating Open GL per-triangle/strip renderer");
		renderer = rend_GLES2();
		break;
#if defined(HAVE_OIT)
	case 3:
		NOTICE_LOG(PVR, "Creating Open GL per-pixel renderer");
		renderer = rend_GL4();
		fallback_renderer = rend_GLES2();
		break;
#endif
#ifdef HAVE_VULKAN
	case 4:
		NOTICE_LOG(PVR, "Creating Vulkan per-triangle/strip renderer");
		renderer = rend_Vulkan();
		break;
	case 5:
		ERROR_LOG(PVR, "Creating Vulkan per-pixel renderer (not implemented)");
//		renderer = rend_OITVulkan();
		break;
#endif
	}
#endif
}

void rend_init_renderer()
{
	if (!renderer->Init())
    {
		delete renderer;
    	if (fallback_renderer == NULL || !fallback_renderer->Init())
    	{
    		if (fallback_renderer != NULL)
    			delete fallback_renderer;
    		die("Renderer initialization failed\n");
    	}
    	INFO_LOG(PVR, "Selected renderer initialization failed. Falling back to default renderer.");
    	renderer  = fallback_renderer;
    	fallback_renderer = NULL;	// avoid double-free
    }
}

void rend_term_renderer()
{
	renderer->Term();
	delete renderer;
	renderer = NULL;
	if (fallback_renderer != NULL)
	{
		delete fallback_renderer;
		fallback_renderer = NULL;
	}
}

bool rend_frame(TA_context* ctx, bool draw_osd)
{
   if (renderer_changed || renderer == NULL)
   {
	  renderer_changed = false;
	  if (renderer != NULL)
		 rend_term_renderer();
	  rend_create_renderer();
	  rend_init_renderer();
   }
   bool proc = renderer->Process(ctx);
#if !defined(TARGET_NO_THREADS)
   if (settings.rend.ThreadedRendering && (!proc || (!ctx->rend.isRenderFramebuffer && !ctx->rend.isRTT)))
	   // If rendering to texture, continue locking until the frame is rendered
      re.Set();
#endif
   
   bool do_swp = proc && renderer->Render();

   return do_swp;
}

bool rend_single_frame(void)
{
	while (true)
	{
		//wait render start only if no frame pending
		if (_pvrrc == NULL)
		{
			do
			{
#if !defined(TARGET_NO_THREADS)
				if (settings.rend.ThreadedRendering)
				{
					if (!rs.Wait(100))
						return false;
					if (do_swap)
					{
						do_swap = false;
						rs.Set();	// set the semaphore in case a render is pending
						return true;
					}
				}
#endif
				_pvrrc = DequeueRender();

				if (!settings.rend.ThreadedRendering && _pvrrc == NULL)
					return false;
			}
			while (!_pvrrc);
		}
		if ((_pvrrc->rend.isRTT || _pvrrc->rend.isRenderFramebuffer) && swap_pending)
		{
			// If there is a frame swap pending, we want to do it now.
			// The current frame "swapping" detection mechanism (using FB_R_SOF1) doesn't work
			// if a RTT frame is rendered in between.
			swap_pending = false;
			return true;
		}

		bool do_swp = rend_frame(_pvrrc, true);
		swap_pending = do_swp && !_pvrrc->rend.isRenderFramebuffer && FB_R_SOF1 != FB_W_SOF1
				 && settings.rend.ThreadedRendering && settings.rend.DelayFrameSwapping;

		if (settings.rend.ThreadedRendering && _pvrrc->rend.isRTT)
			re.Set();

		//clear up & free data ..
		FinishRender(_pvrrc);
		_pvrrc=0;

		if (do_swp && !swap_pending)
			return true;
	}
}

void rend_resize(int width, int height)
{
	renderer->Resize(width, height);
}

void rend_start_render(void)
{
   render_called = true;
   pend_rend = false;
   TA_context* ctx = tactx_Pop(CORE_CURRENT_CTX);

   // No end of render interrupt when rendering the framebuffer
	if (!ctx || !ctx->rend.isRenderFramebuffer)
		SetREP(ctx);

   if (ctx)
   {
      bool is_rtt=(FB_W_SOF1& 0x1000000)!=0 && !ctx->rend.isRenderFramebuffer;

      if (!ctx->rend.Overrun)
      {
         //printf("REP: %.2f ms\n",render_end_pending_cycles/200000.0);
         if (!ctx->rend.isRenderFramebuffer)
            FillBGP(ctx);

         ctx->rend.isRTT      = is_rtt;

         ctx->rend.fb_X_CLIP  = FB_X_CLIP;
         ctx->rend.fb_Y_CLIP  = FB_Y_CLIP;

         ctx->rend.fog_clamp_min = FOG_CLAMP_MIN;
			ctx->rend.fog_clamp_max = FOG_CLAMP_MAX;

         max_idx              = max(max_idx,  ctx->rend.idx.used());
         max_vtx              = max(max_vtx,  ctx->rend.verts.used());
         max_op               = max(max_op,   ctx->rend.global_param_op.used());
         max_pt               = max(max_pt,   ctx->rend.global_param_pt.used());
         max_tr               = max(max_tr,   ctx->rend.global_param_tr.used());

         max_mvo              = max(max_mvo,  ctx->rend.global_param_mvo.used());
         max_modt             = max(max_modt, ctx->rend.modtrig.used());

         if (QueueRender(ctx))
         {
            palette_update();
#if !defined(TARGET_NO_THREADS)
            if (settings.rend.ThreadedRendering)
            	rs.Set();
            else
#endif
            	rend_single_frame();
            pend_rend = true;
         }
      }
      else
      {
         ovrn++;
         INFO_LOG(PVR, "WARNING: Rendering context is overrun (%d), aborting frame", ovrn);
         tactx_Recycle(ctx);
      }
   }
}

void rend_end_render(void)
{
   if (pend_rend)
   {
#if !defined(TARGET_NO_THREADS)
	   if (settings.rend.ThreadedRendering)
		   re.Wait();
	   else
#endif
		  if(renderer != NULL)
			 renderer->Present();
   }
}

void rend_cancel_emu_wait()
{
#if !defined(TARGET_NO_THREADS)
	if (settings.rend.ThreadedRendering)
	{
		rs.Set();
		re.Set();
	}
#endif
	frame_finished.Set();
}

bool rend_init(void)
{
   rend_create_renderer();

#if !defined(TARGET_NO_THREADS)
	if (!settings.rend.ThreadedRendering)
#endif
	{
	   rend_init_renderer();

	   renderer->Resize(screen_width, screen_height);
	}

#if SET_AFNT
	cpu_set_t mask;

	/* CPU_ZERO initializes all the bits in the mask to zero. */
	CPU_ZERO( &mask );
	/* CPU_SET sets only the bit corresponding to cpu. */
	CPU_SET( 0, &mask );

	/* sched_setaffinity returns 0 in success */

	if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )
		WARN_LOG(PVR, "WARNING: Could not set CPU Affinity, continuing...");
#endif

	return true;
}

void rend_term(void)
{
}

void rend_vblank()
{
   if (!render_called && fb_dirty && FB_R_CTRL.fb_enable)
	{
		DEBUG_LOG(PVR, "Direct framebuffer write detected");
		u32 saved_ctx_addr = PARAM_BASE;
		bool restore_ctx = ta_ctx != NULL;
		PARAM_BASE = 0xF00000;
		SetCurrentTARC(CORE_CURRENT_CTX);
		ta_ctx->Reset();
		ta_ctx->rend.isRenderFramebuffer = true;
		ta_ctx->rend.isRTT = false;
		rend_start_render();
		PARAM_BASE = saved_ctx_addr;
		if (restore_ctx)
			SetCurrentTARC(CORE_CURRENT_CTX);
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();
	cheatManager.Apply();

   os_DoEvents();
}

void check_framebuffer_write()
{
   u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) * 4;
	fb_watch_addr_start = (SPG_CONTROL.interlace ? FB_R_SOF2 : FB_R_SOF1) & VRAM_MASK;
	fb_watch_addr_end = fb_watch_addr_start + fb_size;
}

void rend_swap_frame()
{
	if (swap_pending)
	{
		swap_pending = false;
		do_swap = true;
		rs.Set();
	}
}
