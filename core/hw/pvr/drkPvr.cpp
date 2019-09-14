// drkPvr.cpp : Defines the entry point for the DLL application.
//

/*
	Plugin structure
	Interface
	SPG
	TA
	Renderer
*/

#include "drkPvr.h"
#include "ta.h"
#include "spg.h"
#include "pvr_regs.h"
#include "pvr_mem.h"
#include "Renderer_if.h"
#include "rend/gles/CustomTexture.h"

extern CustomTexture custom_texture;

void libPvr_LockedBlockWrite (vram_block* block,u32 addr)
{
	rend_text_invl(block);
}


void libPvr_Reset(bool Manual)
{
   Regs_Reset(Manual);
   spg_Reset(Manual);
}


s32 libPvr_Init(void)
{
   if (!spg_Init())
   {
      //failed
      return -1;
   }

	return 0;
}

//called when exiting from sh4 thread , from the new thread context (for any thread specific de init) :P
void libPvr_Term(void)
{
   custom_texture.Terminate();	// Avoid deadlock on exit (win32)
   rend_term();
   spg_Term();
}
