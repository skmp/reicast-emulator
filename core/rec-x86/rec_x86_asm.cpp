#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86

#include "rec_x86_ngen.h"

u32 gas_offs=offsetof(Sh4RCB,cntx.jdyn);

u32 cpurun_offset=offsetof(Sh4RCB,cntx.CpuRunning);

void (*ngen_FailedToFindBlock)()=&ngen_FailedToFindBlock_;
#endif

