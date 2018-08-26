#include "types.h"

#include <set>
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"

int cycle_counter;


void ngen_init(void)
{
   switch (settings.dynarec.Type)
   {
      case 0: /* native dynarec */
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_init_x86_32bit(void);
         ngen_init_x86_32bit();
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_init_arm(void);
         ngen_init_arm();
#endif
         break;
      case 1: /* rec_cpp */
         break;
   }
}

#if !(FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86)
#endif

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}

int idxnxx = 0;

void ngen_ResetBlocks(void)
{
#ifndef NDEBUG
   printf("@@\tngen_ResetBlocks()\n");
#endif
	idxnxx = 0;
}

void ngen_CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
{
   switch (settings.dynarec.Type)
   {
      case 0:
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_CC_Param_x86(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         ngen_CC_Param_x86(op, par, tp);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_CC_Param_arm(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         ngen_CC_Param_arm(op, par, tp);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_CC_Param_x64(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         ngen_CC_Param_x64(op, par, tp);
         break;
#elif defined(TARGET_NO_JIT)
         /* we want this to fall through */
#else
         break;
#endif
      case 1: /* rec_cpp */
#ifdef TARGET_NO_JIT
         extern void ngen_CC_Param_cpp(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         ngen_CC_Param_cpp(op, par, tp);
#endif
         break;
   }

}

void ngen_CC_Call(shil_opcode*op, void* function)
{
   switch (settings.dynarec.Type)
   {
      case 0:
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_CC_Call_x86(shil_opcode*op,void* function);
         ngen_CC_Call_x86(op, function);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_CC_Call_arm(shil_opcode* op,void* function);
         ngen_CC_Call_arm(op, function);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_CC_Call_x64(shil_opcode*op, void* function);
         ngen_CC_Call_x64(op, function);
         break;
#elif defined(TARGET_NO_JIT)
         /* we want to fall-through here */
#else
         break;
#endif
      case 1:
#ifdef TARGET_NO_JIT
         extern void ngen_CC_Call_cpp(shil_opcode*op, void* function);
         ngen_CC_Call_cpp(op, function);
#endif
         break;
   }
}

void ngen_CC_Finish(shil_opcode* op) 
{
   switch (settings.dynarec.Type)
   {
      case 0:
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_CC_Finish_x86(shil_opcode* op);
         ngen_CC_Finish_x86(op);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_CC_Finish_arm(shil_opcode* op);
         ngen_CC_Finish_arm(op);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_CC_Finish_x64(shil_opcode* op);
         ngen_CC_Finish_x64(op);
         break;
#elif defined(TARGET_NO_JIT)
         /* we want to fall-through here */
#else
         break;
#endif
      case 1:
#ifdef TARGET_NO_JIT
         extern void ngen_CC_Finish_cpp(shil_opcode* op);
         ngen_CC_Finish_cpp(op);
#endif
         break;
   }
}

void ngen_CC_Start(shil_opcode* op) 
{ 
   switch (settings.dynarec.Type)
   {
      case 0:
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_CC_Start_x86(shil_opcode* op);
         ngen_CC_Start_x86(op);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_CC_Start_arm(shil_opcode* op);
         ngen_CC_Start_arm(op);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_CC_Start_x64(shil_opcode* op);
         ngen_CC_Start_x64(op);
         break;
#elif defined(TARGET_NO_JIT)
         /* we want this to fall through */
#else
         break;
#endif
      case 1: /* rec_cpp */
#ifdef TARGET_NO_JIT
         extern void ngen_CC_Start_cpp(shil_opcode* op);
         ngen_CC_Start_cpp(op);
#endif
         break;
   }

}

void ngen_Compile(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise)
{
   switch (settings.dynarec.Type)
   {
      case 0:
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_Compile_x86(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
         ngen_Compile_x86(block, force_checks, reset, staging, optimise);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_Compile_arm(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
         ngen_Compile_arm(block, force_checks, reset, staging, optimise);
         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_Compile_x64(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise);
         ngen_Compile_x64(block, force_checks, reset, staging, optimise);
         break;
#elif defined(TARGET_NO_JIT)
         /* we want this to fall through */
#else
         break;
#endif
      case 1: /* rec_cpp */
#ifdef TARGET_NO_JIT
         extern void ngen_Compile_cpp(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise);
         ngen_Compile_cpp(block, force_checks, reset, staging, optimise);
#endif
         break;
   }
}

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}
