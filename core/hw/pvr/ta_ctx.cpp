#include "ta.h"
#include "ta_ctx.h"

#include "hw/sh4/sh4_sched.h"

extern u32 FrameCount;

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

#if !defined(TARGET_NO_THREADS)
slock_t *mtx_rqueue;
slock_t *mtx_pool;
cResetEvent frame_finished(false, true);
#endif

TA_context* rqueue;

/* texture cache entry pool. */
vector<TA_context*> ctx_pool;
vector<TA_context*> ctx_list;

static TA_context* tactx_Alloc(void)
{
	TA_context* rv = 0;

#ifndef TARGET_NO_THREADS
   slock_lock(mtx_pool);
#endif

	if (ctx_pool.size())
	{
		rv = ctx_pool[ctx_pool.size()-1];
		ctx_pool.pop_back();
	}

#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_pool);
#endif
	
	if (rv)
      return rv;

   rv = new TA_context();
   rv->Alloc();
   printf("new tactx\n");

	return rv;
}

static TA_context* tactx_Find(u32 addr, bool allocnew)
{
   TA_context *rv = NULL;
   for (size_t i=0; i<ctx_list.size(); i++)
   {
      if (ctx_list[i]->Address==addr)
         return ctx_list[i];
   }

   if (!allocnew)
      return 0;

   rv = tactx_Alloc();
   rv->Address=addr;
   ctx_list.push_back(rv);

   return rv;
}

void SetCurrentTARC(u32 addr)
{
	if (addr != TACTX_NONE)
	{
		if (ta_ctx)
			SetCurrentTARC(TACTX_NONE);

		//set new context
		ta_ctx = tactx_Find(addr,true);

		//copy cached params
		ta_tad = ta_ctx->tad;
	}
	else
	{
		//Flush cache to context
		ta_ctx->tad=ta_tad;
		
		//clear context
		ta_ctx=0;
      ta_tad.thd_data = ta_tad.thd_root = ta_tad.thd_old_data = NULL;
	}
}

static bool TryDecodeTARC(void)
{
	if (vd_ctx == 0)
	{
		vd_ctx = ta_ctx;

		vd_ctx->rend.proc_start = vd_ctx->rend.proc_end + 32;
		vd_ctx->rend.proc_end = vd_ctx->tad.thd_data;
			
#ifndef TARGET_NO_THREADS
      slock_lock(vd_ctx->rend_inuse);
#endif
		vd_rc = vd_ctx->rend;

		//signal the vdec thread
		return true;
	}
   return false;
}


#ifdef TARGET_NO_THREADS
static void VDecEnd(void)
{
	vd_ctx->rend = vd_rc;
	vd_ctx = 0;
}

bool QueueRender(TA_context* ctx)
{
	if (rqueue)
   {
		tactx_Recycle(ctx);
		return false;
	}

	TA_context* old = rqueue;
	rqueue=ctx;

	return true;
}

TA_context* DequeueRender(void)
{
	TA_context* rv = rqueue;

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending(void)
{
	TA_context* rv = rqueue;
	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
	rqueue = 0;

	tactx_Recycle(ctx);
}

void ta_ctx_free(void) { }
void ta_ctx_init(void) { }

void tactx_Recycle(TA_context* poped_ctx)
{
   if (ctx_pool.size()>2)
   {
      poped_ctx->Free();
      delete poped_ctx;
   }
   else
   {
      poped_ctx->Reset();
      ctx_pool.push_back(poped_ctx);
   }
}
#else
static void VDecEnd(void)
{
	vd_ctx->rend = vd_rc;

   slock_unlock(vd_ctx->rend_inuse);

	vd_ctx = 0;
}

bool QueueRender(TA_context* ctx)
{
	if (rqueue)
   {
		tactx_Recycle(ctx);
		return false;
	}

	frame_finished.Reset();
   slock_lock(mtx_rqueue);
	TA_context* old = rqueue;
	rqueue=ctx;
   slock_unlock(mtx_rqueue);

	return true;
}

TA_context* DequeueRender(void)
{
   slock_lock(mtx_rqueue);
	TA_context* rv = rqueue;
   slock_unlock(mtx_rqueue);

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending(void)
{
   slock_lock(mtx_rqueue);
	TA_context* rv = rqueue;
   slock_unlock(mtx_rqueue);

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
   slock_lock(mtx_rqueue);
	rqueue = 0;
   slock_unlock(mtx_rqueue);

	tactx_Recycle(ctx);
	frame_finished.Set();
}

void ta_ctx_free(void)
{
   slock_free(mtx_rqueue);
   slock_free(mtx_pool);
   mtx_rqueue = NULL;
   mtx_pool   = NULL;
}

void ta_ctx_init(void)
{
   mtx_rqueue = slock_new();
   mtx_pool   = slock_new();
}

void tactx_Recycle(TA_context* poped_ctx)
{
   slock_lock(mtx_pool);
   if (ctx_pool.size()>2)
   {
      poped_ctx->Free();
      delete poped_ctx;
   }
   else
   {
      poped_ctx->Reset();
      ctx_pool.push_back(poped_ctx);
   }
   slock_unlock(mtx_pool);
}
#endif

TA_context* tactx_Pop(u32 addr)
{
	for (size_t i=0; i<ctx_list.size(); i++)
   {
      TA_context *rv = NULL;
      if (ctx_list[i]->Address != addr)
         continue;

      rv = ctx_list[i];

      if (ta_ctx == rv)
         SetCurrentTARC(TACTX_NONE);

      ctx_list.erase(ctx_list.begin() + i);

      return rv;
   }
	return 0;
}


