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

		verify(ta_ctx == 0);
		//set new context
		ta_ctx = tactx_Find(addr,true);

		//copy cached params
		ta_tad = ta_ctx->tad;
	}
	else
	{
		//Flush cache to context
		verify(ta_ctx != 0);
		ta_ctx->tad=ta_tad;
		
		//clear context
		ta_ctx=0;
		ta_tad.Reset(0);
	}
}

static bool TryDecodeTARC(void)
{
	verify(ta_ctx != 0);

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

static void VDecEnd(void)
{
	verify(vd_ctx != 0);

	vd_ctx->rend = vd_rc;

#ifndef TARGET_NO_THREADS
   slock_unlock(vd_ctx->rend_inuse);
#endif

	vd_ctx = 0;
}


bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
	if (rqueue)
   {
		tactx_Recycle(ctx);
		return false;
	}

#ifndef TARGET_NO_THREADS
	frame_finished.Reset();
   slock_lock(mtx_rqueue);
#endif
	TA_context* old = rqueue;
	rqueue=ctx;
#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_rqueue);
#endif

	verify(!old);

	return true;
}

TA_context* DequeueRender(void)
{
#ifndef TARGET_NO_THREADS
   slock_lock(mtx_rqueue);
#endif
	TA_context* rv = rqueue;
#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_rqueue);
#endif

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending(void)
{
#ifndef TARGET_NO_THREADS
   slock_lock(mtx_rqueue);
#endif
	TA_context* rv = rqueue;
#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_rqueue);
#endif

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
	verify(rqueue == ctx);
#ifndef TARGET_NO_THREADS
   slock_lock(mtx_rqueue);
#endif
	rqueue = 0;
#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_rqueue);
#endif

	tactx_Recycle(ctx);
#ifndef TARGET_NO_THREADS
	frame_finished.Set();
#endif
}



void tactx_Recycle(TA_context* poped_ctx)
{
#ifndef TARGET_NO_THREADS
   slock_lock(mtx_pool);
#endif
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
#ifndef TARGET_NO_THREADS
   slock_unlock(mtx_pool);
#endif
}


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

void ta_ctx_free(void)
{
#ifndef TARGET_NO_THREADS
   slock_free(mtx_rqueue);
   slock_free(mtx_pool);
   mtx_rqueue = NULL;
   mtx_pool   = NULL;
#endif
}

void ta_ctx_init(void)
{
#ifndef TARGET_NO_THREADS
   mtx_rqueue = slock_new();
   mtx_pool   = slock_new();
#endif
}
