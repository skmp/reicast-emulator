#include "types.h"
#include "pvr.h"
#include "spg.h"
#include "ta.h"
#include "hw/mem/_vmem.h"

#include "ta.h"
#include "tr.h"

//TODO : move code later to a plugin
//TODO : Fix registers arrays , they must be smaller now doe to the way SB registers are handled
#include "hw/holly/holly.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mem.h"

#include <rthreads/rthreads.h>

void libCore_vramlock_Free(void)
{
#ifndef TARGET_NO_THREADS
   slock_free(vramlist_lock);
   vramlist_lock = NULL;
#endif
}

void libCore_vramlock_Init(void)
{
#ifndef TARGET_NO_THREADS
   vramlist_lock = slock_new();
#endif
}
