// drkPvr.cpp : Defines the entry point for the DLL application.
//

/*
	Plugin structure
	Interface
	SPG
	TA
	Renderer
*/

#include "types.h"
#include "pvr.h"
#include "spg.h"
#include "ta.h"
#include "hw/mem/_vmem.h"

#include "ta.h"
#include "tr.h"
#include "hw/mem/_vmem.h"

void libPvr_LockedBlockWrite (vram_block* block,u32 addr)
{
	rend_text_invl(block);
}


void libPvr_Reset(bool Manual)
{
   Regs_Reset(Manual);
	CalculateSync();
	//rend_reset(); //*TODO* wtf ?
}


s32 libPvr_Init(void)
{
   ta_ctx_init();
   
   spg_Init();

   //failed
	if (!rend_init())
		return rv_error;

	return rv_ok;
}

//called when exiting from sh4 thread , from the new thread context (for any thread specific de init) :P
void libPvr_Term(void)
{
	rend_term();
   ta_ctx_free();
}
