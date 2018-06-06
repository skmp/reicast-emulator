#include "pvr.h"

#include "hw/sh4/sh4_sched.h"

extern u32 FrameCount;

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

// helper for 32 byte aligned memory allocation
void* OS_aligned_malloc(size_t align, size_t size)
{
#ifdef __MINGW32__
   return __mingw_aligned_malloc(size, align);
#elif defined(_WIN32)
   return _aligned_malloc(size, align);
#else
   void *p = NULL;
   int ret = posix_memalign(&p, align, size);
   return (ret == 0) ? p : 0;
#endif
}

// helper for 32 byte aligned memory de-allocation
void OS_aligned_free(void *ptr)
{
#ifdef __MINGW32__
   __mingw_aligned_free(ptr);
#elif defined(_MSC_VER)
   _aligned_free(ptr);
#else
   free(ptr);
#endif
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
      ta_tad.Reset(0);
	}
}

bool TryDecodeTARC(void)
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

void VDecEnd(void)
{
	vd_ctx->rend = vd_rc;

#ifndef TARGET_NO_THREADS
   slock_unlock(vd_ctx->rend_inuse);
#endif

	vd_ctx = 0;
}

#if !defined(TARGET_NO_THREADS)
slock_t *mtx_rqueue;
#endif
TA_context* rqueue;
#if !defined(TARGET_NO_THREADS)
cResetEvent frame_finished;
#endif

bool QueueRender(TA_context* ctx)
{
	if (rqueue)
   {
		tactx_Recycle(ctx);
		return false;
	}

#if !defined(TARGET_NO_THREADS)
   slock_lock(frame_finished.mutx);
   frame_finished.state = false;
   slock_unlock(frame_finished.mutx);

   slock_lock(mtx_rqueue);
#endif
	TA_context* old = rqueue;
	rqueue=ctx;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mtx_rqueue);
#endif

	return true;
}

TA_context* DequeueRender(void)
{
#if !defined(TARGET_NO_THREADS)
   slock_lock(mtx_rqueue);
#endif
	TA_context* rv = rqueue;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mtx_rqueue);
#endif

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending(void)
{
#if !defined(TARGET_NO_THREADS)
   slock_lock(mtx_rqueue);
#endif
	TA_context* rv = rqueue;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mtx_rqueue);
#endif

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
#if !defined(TARGET_NO_THREADS)
   slock_lock(mtx_rqueue);
#endif
	rqueue = 0;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mtx_rqueue);
#endif

	tactx_Recycle(ctx);

#if !defined(TARGET_NO_THREADS)
   slock_lock(frame_finished.mutx);
   frame_finished.state = true;
   scond_signal(frame_finished.cond);
   slock_unlock(frame_finished.mutx);
#endif
}

#if !defined(TARGET_NO_THREADS)
slock_t *mtx_pool;
#endif

/* texture cache entry pool. */
vector<TA_context*> ctx_pool;
vector<TA_context*> ctx_list;

TA_context* tactx_Alloc(void)
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

void tactx_Recycle(TA_context* poped_ctx)
{
#if !defined(TARGET_NO_THREADS)
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
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mtx_pool);
#endif
}

TA_context* tactx_Find(u32 addr, bool allocnew)
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
#if !defined(TARGET_NO_THREADS)
   slock_free(mtx_rqueue);
   slock_free(mtx_pool);
   mtx_rqueue = NULL;
   mtx_pool   = NULL;
#endif
}

void ta_ctx_init(void)
{
#if !defined(TARGET_NO_THREADS)
   mtx_rqueue = slock_new();
   mtx_pool   = slock_new();
#endif
}
