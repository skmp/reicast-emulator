#include "pvr.h"

#include "hw/sh4/sh4_sched.h"

extern u32 FrameCount;

TA_context* ta_ctx;
tad_context ta_tad;

TA_context*  vd_ctx;
rend_context vd_rc;

#if !defined(TARGET_NO_THREADS)
slock_t *mtx_rqueue;
slock_t *mtx_pool;
cResetEvent frame_finished;
#endif

#if HOST_CPU == CPU_X86
#include <xmmintrin.h>
struct simd256_t
{
   DECL_ALIGN(32) __m128 data[2];
};
#elif HOST_CPU == CPU_ARM && defined(__ARM_NEON__)
#include <arm_neon.h>
struct simd256_t
{
	DECL_ALIGN(32) uint64x2_t data[2];
};
#else
struct simd256_t
{
   DECL_ALIGN(32) u64 data[4];
};
#endif

TA_context* rqueue;

u32 ta_type_lut[256];

/*
	Threaded TA Implementation

	Main thread -> ta data -> stored	(tactx)

	Render/TA thread -> ta data -> draw lists -> draw
*/

#define ta_cur_state  (ta_fsm[2048])

/*
	Partial TA parsing for in emu-side handling. Properly tracks 32/64 byte state, and
	Calls a helper function every time something important (SOL/EOL, etc) happens

	Uses a state machine, with 3 bits state and 8 bits (PT:OBJ[6:2]) input
*/

enum ta_state
{
	               // -> TAS_NS, TAS_PLV32, TAS_PLHV32, TAS_PLV64, TAS_PLHV64, TAS_MLV64
	TAS_NS,        //

	               // -> TAS_NS, TAS_PLV32, TAS_PLHV32, TAS_PLV64, TAS_PLHV64
	TAS_PLV32,     //polygon list PMV<?>, V32
	
	               // -> TAS_NS, TAS_PLV32, TAS_PLHV32, TAS_PLV64, TAS_PLHV64
	TAS_PLV64,     //polygon list PMV<?>, V64

	               // -> TAS_NS, TAS_MLV64, TAS_MLV64_H
	TAS_MLV64,     //mv list

	               // -> TAS_PLV32
	TAS_PLHV32,    //polygon list PMV<64> 2nd half -> V32

	               // -> TAS_PLV64
	TAS_PLHV64,    //polygon list PMV<64> 2nd half -> V64

	               // -> TAS_PLV64_H
	TAS_PLV64_H,   //polygon list V64 2nd half


	               // -> TAS_MLV64
	TAS_MLV64_H    //mv list, 64 bit half
};

const HollyInterruptID ListEndInterrupt[5]=
{
	holly_OPAQUE,
	holly_OPAQUEMOD,
	holly_TRANS,
	holly_TRANSMOD,
	holly_PUNCHTHRU
};

static u8 ta_fsm[2049];	//[2048] stores the current state
static u32 ta_fsm_cl=7;

/* state | PTEOS | OBJ -> next, proc*/


static void fill_fsm(ta_state st, s8 pt, s8 obj, ta_state next, u32 proc=0, u32 sz64=0)
{
   unsigned i, j;
	for (i=0;i<8;i++)
	{
		if (pt != -1) i=pt;

		for (j=0;j<32;j++)
		{
			if (obj != -1)
            j=obj;
			ta_fsm[(st<<8)+(i<<5)+j]=next | proc*16 /*| sz64*32*/;
			if (obj != -1)
            break;
		}

		if (pt != -1)
         break;
	}
}

static void fill_fsm(void)
{
   unsigned i;
	//initialise to invalid
	for (i=0;i<2048;i++)
		ta_fsm[i]=(i>>8) | 0x80;

	for (i=0;i<8;i++)
	{
		switch(i)
      {
         case TA_PARAM_END_OF_LIST:
            //End of list -> process it !
            fill_fsm(TAS_NS,   TA_PARAM_END_OF_LIST,-1,TAS_NS,1);
            fill_fsm(TAS_PLV32,TA_PARAM_END_OF_LIST,-1,TAS_NS,1);
            fill_fsm(TAS_PLV64,TA_PARAM_END_OF_LIST,-1,TAS_NS,1);
            fill_fsm(TAS_MLV64,TA_PARAM_END_OF_LIST,-1,TAS_NS,1);
            break;

         case TA_PARAM_USER_TILE_CLIP:
         case TA_PARAM_OBJ_LIST_SET:
            //32B commands, no state change
            fill_fsm(TAS_NS,i,-1,TAS_NS);
            fill_fsm(TAS_PLV32,i,-1,TAS_PLV32);
            fill_fsm(TAS_PLV64,i,-1,TAS_PLV64);
            fill_fsm(TAS_MLV64,i,-1,TAS_MLV64);
            break;

         case 3:
         case 6:
            //invalid
            break;

         case TA_PARAM_POLY_OR_VOL:
            //right .. its complicated alirte

            for (int k=0;k<32;k++)
            {
               u32 uid=ta_type_lut[k*4];
               u32 vt=uid & 0x7f;

               bool v64 = vt == 5 || vt == 6 || vt == 11 || vt == 12 || vt == 13 || vt == 14;
               bool p64 = uid >> 31;

               ta_state nxt = p64 ? (v64 ? TAS_PLHV64 : TAS_PLHV32) :
                  (v64 ? TAS_PLV64  : TAS_PLV32 ) ;

               fill_fsm(TAS_PLV32,i,k,nxt,0,p64);
               fill_fsm(TAS_PLV64,i,k,nxt,0,p64);
            }


            //32B command, no state change
            fill_fsm(TAS_MLV64,i,-1,TAS_MLV64);

            //process and start list
            fill_fsm(TAS_NS,i,-1,TAS_NS,1);
            break;

         case TA_PARAM_SPRITE:
            //SPR: 32B -> expect 64B data (PL*)
            fill_fsm(TAS_PLV32,i,-1,TAS_PLV64);
            fill_fsm(TAS_PLV64,i,-1,TAS_PLV64);

            //invalid for ML

            //process and start list
            fill_fsm(TAS_NS,i,-1,TAS_NS,1);
            break;

         case TA_PARAM_VERTEX:
            //VTX: 32 B -> Expect more of it
            fill_fsm(TAS_PLV32,i,-1,TAS_PLV32,0,0);

            //VTX: 64 B -> Expect next 32B
            fill_fsm(TAS_PLV64,i,-1,TAS_PLV64_H,0,1);

            //MVO: 64B -> expect next 32B
            fill_fsm(TAS_MLV64,i,-1,TAS_MLV64_H,0,1);

            //invalid for NS
            break;
      }
	}
	//?

	fill_fsm(TAS_PLHV32,-1,-1,TAS_PLV32);  //64 PH -> expect V32
	fill_fsm(TAS_PLHV64,-1,-1,TAS_PLV64);  //64 PH -> expect V64

	fill_fsm(TAS_PLV64_H,-1,-1,TAS_PLV64); //64 VH -> expect V64
	fill_fsm(TAS_MLV64_H,-1,-1,TAS_MLV64); //64 MH -> expect M64
}

static OnLoad ol_fillfsm(&fill_fsm);

void ta_vtx_ListCont(void)
{
	SetCurrentTARC(TA_ISP_BASE);

	ta_cur_state=TAS_NS;
}

void ta_vtx_ListInit(void)
{
	SetCurrentTARC(TA_ISP_BASE);
   ta_tad.ClearPartial();

	ta_cur_state=TAS_NS;
}
void ta_vtx_SoftReset(void)
{
	ta_cur_state=TAS_NS;
}

void ta_vtx_data(u32* data, u32 size)
{
	while(size>0)
   {
      simd256_t *src = (simd256_t*)data; 
      simd256_t *dst = (simd256_t*)ta_tad.thd_data;

      PCW pcw        = *(PCW*)data;

      /* Copy the TA data */
      *dst = *src;

      ta_tad.thd_data+=32;

      /* Process TA state */
      u32 state_in     = (ta_cur_state<<8) | (pcw.ParaType<<5) | (pcw.obj_ctrl>>2)%32;

      u32 trans         = ta_fsm[state_in];
      ta_cur_state     = (ta_state)trans;
      bool must_handle = trans& 0xF0;

      if (must_handle)
      {
         Ta_Dma* dat=(Ta_Dma*)(ta_tad.thd_data-32);

         u32 cmd = trans>>4;
         trans&=7;
         //printf("Process state transition: %d || %d -> %d \n",cmd,state_in,trans&0xF);

         if (cmd != 8)
         {
            switch (dat->pcw.ParaType)
            {
               case TA_PARAM_END_OF_LIST:
                  if (ta_fsm_cl==7)
                     ta_fsm_cl=dat->pcw.ListType;
                  //printf("List %d ended\n",ta_fsm_cl);

                  asic_RaiseInterrupt( ListEndInterrupt[ta_fsm_cl]);
                  ta_fsm_cl=7;
                  trans=TAS_NS;
                  break;
               case TA_PARAM_POLY_OR_VOL:
               case TA_PARAM_SPRITE:
                  if (ta_fsm_cl==7)
                     ta_fsm_cl=dat->pcw.ListType;

                  trans=TAS_PLV32;
                  if (dat->pcw.ParaType == TA_PARAM_POLY_OR_VOL &&
                        IsModVolList(ta_fsm_cl))
                     trans=TAS_MLV64;
                  break;
               default:
                  break;
            }
         }

         u32 state_in = (trans<<8) | (dat->pcw.ParaType<<5) | (dat->pcw.obj_ctrl>>2)%32;
         ta_cur_state = (ta_state)(ta_fsm[state_in]&0xF);
      }

      data+=8;
      size--;
   }
}

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

#ifdef TARGET_NO_THREADS
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

bool QueueRender(TA_context* ctx)
{
	if (rqueue)
   {
		tactx_Recycle(ctx);
		return false;
	}

   slock_lock(frame_finished.mutx);
   frame_finished.state = false;
   slock_unlock(frame_finished.mutx);

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

   slock_lock(frame_finished.mutx);
   frame_finished.state = true;
   scond_signal(frame_finished.cond);
   slock_unlock(frame_finished.mutx);
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
