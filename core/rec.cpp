#include "types.h"

#include <map>
#include <algorithm>

#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"

int cycle_counter;

#if defined(_ANDROID) || defined(IOS)
#define ARM_MOBILE
#endif

struct DynaRBI : RuntimeBlockInfo
{
   /* NOTE/TODO - this was virtual u32 Relink();
    * for rec_arm.cpp - check this in case of
    * errors */
	virtual u32 Relink()
   {
		return 0;
	}

	virtual void Relocate(void* dst) {
	}
};

#if !defined(ARM_MOBILE) || (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86)
static void ngen_mainloop_exec(Sh4RCB* ctx)
{
	cycle_counter = SH4_TIMESLICE;

   do {
      DynarecCodeEntryPtr rcb = bm_GetCode(ctx->cntx.pc);
      rcb();
   } while (cycle_counter > 0);

   if (UpdateSystem())
      rdv_DoInterrupts_pc(ctx->cntx.pc);
}

void ngen_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

   for (
#if defined(TARGET_BOUNDED_EXECUTION)
	int i=0; i<10000; i++
#else
	;;
#endif
   )
      ngen_mainloop_exec(ctx);
}
#endif


void ngen_init(void)
{
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
   extern void ngen_init_x86_32bit(void);
   ngen_init_x86_32bit();
#elif defined(ARM_MOBILE)
   extern void ngen_init_arm(void);
   nngen_init_arm();
#endif
}

#if !(FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86)
extern "C" {

void ngen_FailedToFindBlock_internal(void)
{
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

};

void(*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;
#endif

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}

int idxnxx = 0;

void ngen_ResetBlocks(void)
{
   printf("@@\tngen_ResetBlocks()\n");
	idxnxx = 0;
}

RuntimeBlockInfo* ngen_AllocateBlock()
{
	return new DynaRBI();
}

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}
