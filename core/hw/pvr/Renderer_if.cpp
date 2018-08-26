#include "Renderer_if.h"
#include "ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"

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

#if !defined(TARGET_NO_THREADS)
cResetEvent rs(false,true);
cResetEvent re(false,true);
#endif

int max_idx,max_mvo,max_op,max_pt,max_tr,max_vtx,max_modt, ovrn;
bool pend_rend = false;

static bool render_called = false;
u32 fb1_watch_addr_start;
u32 fb1_watch_addr_end;
u32 fb2_watch_addr_start;
u32 fb2_watch_addr_end;
bool fb_dirty;

TA_context* _pvrrc;
void SetREP(TA_context* cntx);

bool rend_frame(TA_context* ctx, bool draw_osd)
{
   bool proc = renderer->Process(ctx);

#if !defined(TARGET_NO_THREADS)
   if (settings.rend.ThreadedRendering && !ctx->rend.isRenderFramebuffer)
      re.Set();
#endif
   
   bool do_swp = proc && renderer->Render();

   return do_swp;
}

bool rend_single_frame(void)
{
   //wait render start only if no frame pending
   do
   {
#if !defined(TARGET_NO_THREADS)
      if (settings.rend.ThreadedRendering && !rs.Wait(100))
         return false;
#endif
      _pvrrc = DequeueRender();
   }
   while (!_pvrrc);
   bool do_swp = rend_frame(_pvrrc, true);

   //clear up & free data ..
   FinishRender(_pvrrc);
   _pvrrc=0;

   return do_swp;
}

int rend_en = true;

void *rend_thread(void* p)
{
#if SET_AFNT
   cpu_set_t mask;

   /* CPU_ZERO initializes all the bits in the mask to zero. */
   CPU_ZERO( &mask );
   /* CPU_SET sets only the bit corresponding to cpu. */
   CPU_SET( 1, &mask );

   /* sched_setaffinity returns 0 in success */

   if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )
      printf("WARNING: Could not set CPU Affinity, continuing...\n");
#endif

   if (!renderer->Init())
      die("rend->init() failed\n");

   //we don't know if this is true, so let's not speculate here
   //renderer->Resize(640, 480);

   while(rend_en)
   {
      if (rend_single_frame())
         renderer->Present();
   }

   return 0;
}

#if !defined(TARGET_NO_THREADS)
cThread rthd(rend_thread,0);
#endif

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
         printf("WARNING: Rendering context is overrun (%d), aborting frame\n",ovrn);
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
           renderer->Present();
   }
}

void rend_cancel_emu_wait()
{
#if !defined(TARGET_NO_THREADS)
	if (settings.rend.ThreadedRendering)
		re.Set();
#endif
}

bool rend_init(void)
{
#ifdef NO_REND
	renderer	 = rend_norend();
#elif defined(HAVE_GL4)
	renderer = rend_GL4();
#else
	renderer = rend_GLES2();
#endif

#if !defined(TARGET_NO_THREADS)
	if (!settings.rend.ThreadedRendering)
#endif
	{
		if (!renderer->Init())
			die("rend->init() failed\n");

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
		printf("WARNING: Could not set CPU Affinity, continuing...\n");
#endif

	return true;
}

void rend_term(void)
{
}

void rend_terminate(void)
{
   rend_en = false;
}

void rend_vblank()
{
   if (!render_called && fb_dirty && FB_R_CTRL.fb_enable)
	{
		SetCurrentTARC(CORE_CURRENT_CTX);
		ta_ctx->rend.isRenderFramebuffer = true;
		rend_start_render();
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();

   os_DoEvents();
}

void check_framebuffer_write()
{
	u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) / 4;
	fb1_watch_addr_start = FB_R_SOF1;
	fb1_watch_addr_end = FB_R_SOF1 + fb_size - 1;
	fb2_watch_addr_start = FB_R_SOF2;
	fb2_watch_addr_end = FB_R_SOF2 + fb_size - 1;
}
