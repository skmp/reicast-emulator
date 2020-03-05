/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "ta.h"
#include "ta_ctx.h"

#include "hw/sh4/sh4_sched.h"

extern u32 fskip;
extern u32 FrameCount;

int frameskip=0;
bool FrameSkipping=false;		// global switch to enable/disable frameskip

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

#if ANDROID
#include <errno.h>
#include <malloc.h>

int posix_memalign(void** memptr, size_t alignment, size_t size) {
  if ((alignment & (alignment - 1)) != 0 || alignment == 0) {
    return EINVAL;
  }

  if (alignment % sizeof(void*) != 0) {
    return EINVAL;
  }

  *memptr = memalign(alignment, size);
  if (*memptr == NULL) {
    return errno;
  }

  return 0;
}
#endif

// helper for 32 byte aligned memory allocation
void* OS_aligned_malloc(size_t align, size_t size)
{
        void *result;
	#ifdef __MINGW32__
		return __mingw_aligned_malloc(size, align);
        #elif HOST_OS == OS_WINDOWS
                result = _aligned_malloc(size, align);
        #else
                if(posix_memalign(&result, align, size)) result = 0;
        #endif
                return result;
}

// helper for 32 byte aligned memory de-allocation
void OS_aligned_free(void *ptr)
{
	#ifdef __MINGW32__
		__mingw_aligned_free(ptr);
        #elif HOST_OS == OS_WINDOWS
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

bool TryDecodeTARC()
{
	verify(ta_ctx != 0);

	if (vd_ctx == 0)
	{
		vd_ctx = ta_ctx;

		vd_ctx->rend.proc_start = vd_ctx->rend.proc_end + 32;
		vd_ctx->rend.proc_end = vd_ctx->tad.thd_data;
			
		vd_ctx->rend_inuse.Lock();
		vd_rc = vd_ctx->rend;

		//signal the vdec thread
		return true;
	}
	else
		return false;
}

void VDecEnd()
{
	verify(vd_ctx != 0);

	vd_ctx->rend = vd_rc;

	vd_ctx->rend_inuse.Unlock();

	vd_ctx = 0;
}

cMutex mtx_rqueue;
TA_context* rqueue;
cResetEvent frame_finished;

double last_frame = 0;
u64 last_cyces = 0;

bool QueueRender(TA_context* ctx)
{
	verify(ctx != 0);
	
	if (FrameSkipping && frameskip) {
 		frameskip=1-frameskip;
		tactx_Recycle(ctx);
		fskip++;
		return false;
 	}
 	
 	//Try to limit speed to a "sane" level
 	//Speed is also limited via audio, but audio
 	//is sometimes not accurate enough (android, vista+)
 	u32 cycle_span = (u32)(sh4_sched_now64() - last_cyces);
 	last_cyces = sh4_sched_now64();
 	double time_span = os_GetSeconds() - last_frame;
 	last_frame = os_GetSeconds();

 	bool too_fast = (cycle_span / time_span) > (SH4_MAIN_CLOCK * 1.2);
	
	if (rqueue && too_fast && settings.pvr.SynchronousRender) {
		//wait for a frame if
		//  we have another one queue'd and
		//  sh4 run at > 120% on the last slice
		//  and SynchronousRendering is enabled
		frame_finished.Wait();
	}

	if (rqueue) {
		// FIXME if the discarded render is a RTT we'll have a texture missing. But waiting for the current frame to finish kills performance...
		tactx_Recycle(ctx);
		fskip++;
		return false;
	}

	frame_finished.Reset();
	mtx_rqueue.Lock();
	TA_context* old = rqueue;
	rqueue=ctx;
	mtx_rqueue.Unlock();

	verify(!old);

	return true;
}

TA_context* DequeueRender()
{
	mtx_rqueue.Lock();
	TA_context* rv = rqueue;
	mtx_rqueue.Unlock();

	if (rv)
		FrameCount++;

	return rv;
}

bool rend_framePending() {
	mtx_rqueue.Lock();
	TA_context* rv = rqueue;
	mtx_rqueue.Unlock();

	return rv != 0;
}

void FinishRender(TA_context* ctx)
{
	verify(ctx == NULL || rqueue == ctx);
	mtx_rqueue.Lock();
	rqueue = 0;
	mtx_rqueue.Unlock();

	if (ctx != NULL)
		tactx_Recycle(ctx);
	frame_finished.Set();
}

cMutex mtx_pool;

vector<TA_context*> ctx_pool;
vector<TA_context*> ctx_list;

TA_context* tactx_Alloc()
{
	TA_context* rv = 0;

	mtx_pool.Lock();
	if (ctx_pool.size())
	{
		rv = ctx_pool[ctx_pool.size()-1];
		ctx_pool.pop_back();
	}
	mtx_pool.Unlock();
	
	if (!rv)
	{
		rv = new TA_context();
		rv->Alloc();
	}

	return rv;
}

void tactx_Recycle(TA_context* poped_ctx)
{
	mtx_pool.Lock();
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
	mtx_pool.Unlock();
}

TA_context* tactx_Find(u32 addr, bool allocnew)
{
	for (size_t i=0; i<ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address==addr)
			return ctx_list[i];
	}

	if (allocnew)
	{
		TA_context* rv = tactx_Alloc();
		rv->Address=addr;
		ctx_list.push_back(rv);

		return rv;
	}
	else
	{
		return 0;
	}
}

TA_context* tactx_Pop(u32 addr)
{
	for (size_t i=0; i<ctx_list.size(); i++)
	{
		if (ctx_list[i]->Address==addr)
		{
			TA_context* rv = ctx_list[i];
			
			if (ta_ctx == rv)
				SetCurrentTARC(TACTX_NONE);

			ctx_list.erase(ctx_list.begin() + i);

			return rv;
		}
	}
	return 0;
}

void tactx_Term()
{
	for (size_t i = 0; i < ctx_list.size(); i++)
	{
		ctx_list[i]->Free();
		delete ctx_list[i];
	}
	ctx_list.clear();
	mtx_pool.Lock();
	{
		for (size_t i = 0; i < ctx_pool.size(); i++)
		{
			ctx_pool[i]->Free();
			delete ctx_pool[i];
		}
	}
	ctx_pool.clear();
	mtx_pool.Unlock();
}

#include "deps/zlib/zlib.h"
#include "hw/pvr/pvr_mem.h"

void tactx_write_frame(const char* file, TA_context* ctx, u8* vram, u8* vram_ref) {
    FILE* fw = fopen(file, "wb");

    //append to it
    fseek(fw, 0, SEEK_END);

    u32 bytes = (u32)(ctx->tad.End() - ctx->tad.thd_root);

    fwrite("TAFRAME4", 1, 8, fw);

    fwrite(&ctx->rend.isRTT, 1, sizeof(ctx->rend.isRTT), fw);
    u32 zero = 0;
    fwrite(&zero, 1, sizeof(bool), fw);	// Was autosort
    fwrite(&ctx->rend.fb_X_CLIP.full, 1, sizeof(ctx->rend.fb_X_CLIP.full), fw);
    fwrite(&ctx->rend.fb_Y_CLIP.full, 1, sizeof(ctx->rend.fb_Y_CLIP.full), fw);

    fwrite(ctx->rend.global_param_op.head(), 1, sizeof(PolyParam), fw);
    fwrite(ctx->rend.verts.head(), 1, 4 * sizeof(Vertex), fw);

    u32 t = VRAM_SIZE;
    fwrite(&t, 1, sizeof(t), fw);

    u8* compressed;
    uLongf compressed_size;
    u8* src_vram = vram;

    if (vram_ref) {
        src_vram = (u8*)malloc(VRAM_SIZE);

        for (int i = 0; i < VRAM_SIZE; i++) {
            src_vram[i] = vram[i] ^ vram_ref[i];
        }
    }

    compressed = (u8*)malloc(VRAM_SIZE + 16);
    compressed_size = VRAM_SIZE;
    verify(compress(compressed, &compressed_size, src_vram, VRAM_SIZE) == Z_OK);
    fwrite(&compressed_size, 1, sizeof(compressed_size), fw);
    fwrite(compressed, 1, compressed_size, fw);
    free(compressed);

    if (src_vram != vram)
        free(src_vram);

    fwrite(&bytes, 1, sizeof(t), fw);
    compressed = (u8*)malloc(bytes + 16);
    compressed_size = bytes;
    verify(compress(compressed, &compressed_size, ctx->tad.thd_root, bytes) == Z_OK);
    fwrite(&compressed_size, 1, sizeof(compressed_size), fw);
    fwrite(compressed, 1, compressed_size, fw);
    free(compressed);

    fwrite(&ctx->tad.render_pass_count, 1, sizeof(u32), fw);
    for (int i = 0; i < ctx->tad.render_pass_count; i++) {
        u32 offset = (u32)(ctx->tad.render_passes[i] - ctx->tad.thd_root);
        fwrite(&offset, 1, sizeof(offset), fw);
    }

    fwrite(pvr_regs, 1, sizeof(pvr_regs), fw);

    fclose(fw);
}

TA_context* tactx_read_frame(const char* file, u8* vram, u8* vram_ref) {

    FILE* fw = fopen(file, "rb");
    if (fw == NULL)
        die("Cannot open frame to display");
    char id0[8] = { 0 };
    u32 t = 0;

    fread(id0, 1, 8, fw);

    if (memcmp(id0, "TAFRAME", 7) != 0 || (id0[7] != '3' && id0[7] != '4')) {
        fclose(fw);
        return 0;
    }
    int sizeofPolyParam = sizeof(PolyParam);
    int sizeofVertex = sizeof(Vertex);
    if (id0[7] == '3')
    {
        sizeofPolyParam -= 12;
        sizeofVertex -= 16;
    }

    TA_context* ctx = tactx_Alloc();

    ctx->Reset();

    ctx->tad.Clear();

    fread(&ctx->rend.isRTT, 1, sizeof(ctx->rend.isRTT), fw);
    fread(&t, 1, sizeof(bool), fw);	// Was autosort
    fread(&ctx->rend.fb_X_CLIP.full, 1, sizeof(ctx->rend.fb_X_CLIP.full), fw);
    fread(&ctx->rend.fb_Y_CLIP.full, 1, sizeof(ctx->rend.fb_Y_CLIP.full), fw);

    fread(ctx->rend.global_param_op.Append(), 1, sizeofPolyParam, fw);
    Vertex* vtx = ctx->rend.verts.Append(4);
    for (int i = 0; i < 4; i++)
        fread(vtx + i, 1, sizeofVertex, fw);

    fread(&t, 1, sizeof(t), fw);
    verify(t == VRAM_SIZE);

    uLongf compressed_size;

    fread(&compressed_size, 1, sizeof(compressed_size), fw);

    u8* gz_stream = (u8*)malloc(compressed_size);
    fread(gz_stream, 1, compressed_size, fw);
    uLongf tl = t;
    verify(uncompress(vram, &tl, gz_stream, compressed_size) == Z_OK);
    free(gz_stream);

    fread(&t, 1, sizeof(t), fw);
    fread(&compressed_size, 1, sizeof(compressed_size), fw);
    gz_stream = (u8*)malloc(compressed_size);
    fread(gz_stream, 1, compressed_size, fw);
    tl = t;
    verify(uncompress(ctx->tad.thd_data, &tl, gz_stream, compressed_size) == Z_OK);
    free(gz_stream);

    ctx->tad.thd_data += t;

    if (fread(&t, 1, sizeof(t), fw) > 0) {
        ctx->tad.render_pass_count = t;
        for (int i = 0; i < t; i++) {
            u32 offset;
            fread(&offset, 1, sizeof(offset), fw);
            ctx->tad.render_passes[i] = ctx->tad.thd_root + offset;
        }
    }
    fread(pvr_regs, 1, sizeof(pvr_regs), fw);

    fclose(fw);

    return ctx;
}
