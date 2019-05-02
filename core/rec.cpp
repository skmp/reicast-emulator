#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"

int cycle_counter;

void (*ngen_Compile)(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
void (*ngen_CC_Start)(shil_opcode* op);
void (*ngen_CC_Call)(shil_opcode*op, void* function);
void (*ngen_CC_Param)(shil_opcode* op, shil_param* par, CanonicalParamType tp);
void (*ngen_CC_Finish)(shil_opcode* op);

void ngen_init(void)
{
   switch (settings.dynarec.Type)
   {
      case 0: /* native dynarec */
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
         extern void ngen_init_x86_32bit(void);
         extern void ngen_Compile_x86(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
         extern void ngen_CC_Start_x86(shil_opcode* op);
         extern void ngen_CC_Call_x86(shil_opcode*op,void* function);
         extern void ngen_CC_Param_x86(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         extern void ngen_CC_Finish_x86(shil_opcode* op);

         ngen_init_x86_32bit();
         ngen_Compile = ngen_Compile_x86;
         ngen_CC_Start = ngen_CC_Start_x86;
         ngen_CC_Call = ngen_CC_Call_x86;
         ngen_CC_Param = ngen_CC_Param_x86;
         ngen_CC_Finish = ngen_CC_Finish_x86;

         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM
         extern void ngen_init_arm(void);
         extern void ngen_Compile_arm(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
         extern void ngen_CC_Start_arm(shil_opcode* op);
         extern void ngen_CC_Call_arm(shil_opcode* op,void* function);
         extern void ngen_CC_Param_arm(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         extern void ngen_CC_Finish_arm(shil_opcode* op);

         ngen_init_arm();
         ngen_Compile = ngen_Compile_arm;
         ngen_CC_Start = ngen_CC_Start_arm;
         ngen_CC_Call = ngen_CC_Call_arm;
         ngen_CC_Param = ngen_CC_Param_arm;
         ngen_CC_Finish = ngen_CC_Finish_arm;

         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_ARM64
         extern void ngen_init_arm64(void);
         extern void ngen_Compile_arm64(RuntimeBlockInfo* block,bool force_checks, bool reset, bool staging,bool optimise);
         extern void ngen_CC_Start_arm64(shil_opcode* op);
         extern void ngen_CC_Call_arm64(shil_opcode* op,void* function);
         extern void ngen_CC_Param_arm64(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         extern void ngen_CC_Finish_arm64(shil_opcode* op);

         ngen_init_arm64();
         ngen_Compile = ngen_Compile_arm64;
         ngen_CC_Start = ngen_CC_Start_arm64;
         ngen_CC_Call = ngen_CC_Call_arm64;
         ngen_CC_Param = ngen_CC_Param_arm64;
         ngen_CC_Finish = ngen_CC_Finish_arm64;

         break;
#elif FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
         extern void ngen_Compile_x64(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise);
         extern void ngen_CC_Start_x64(shil_opcode* op);
         extern void ngen_CC_Call_x64(shil_opcode*op, void* function);
         extern void ngen_CC_Param_x64(shil_opcode* op,shil_param* par,CanonicalParamType tp);
         extern void ngen_CC_Finish_x64(shil_opcode* op);

         ngen_Compile = ngen_Compile_x64;
         ngen_CC_Start = ngen_CC_Start_x64;
         ngen_CC_Call = ngen_CC_Call_x64;
         ngen_CC_Param = ngen_CC_Param_x64;
         ngen_CC_Finish = ngen_CC_Finish_x64;

         break;
#elif defined(TARGET_NO_JIT)
         /* we want this to fall through */
#else
         break;
#endif

      case 1: /* rec_cpp */
#ifdef TARGET_NO_JIT
    	 extern void ngen_Compile_cpp(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise);
    	 extern void ngen_CC_Start_cpp(shil_opcode* op);
    	 extern void ngen_CC_Call_cpp(shil_opcode*op, void* function);
    	 extern void ngen_CC_Param_cpp(shil_opcode* op,shil_param* par,CanonicalParamType tp);
    	 extern void ngen_CC_Finish_cpp(shil_opcode* op);

         ngen_Compile = ngen_Compile_cpp;
         ngen_CC_Start = ngen_CC_Start_cpp;
         ngen_CC_Call = ngen_CC_Call_cpp;
         ngen_CC_Param = ngen_CC_Param_cpp;
         ngen_CC_Finish = ngen_CC_Finish_cpp;
#endif
         break;
   }
}

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
