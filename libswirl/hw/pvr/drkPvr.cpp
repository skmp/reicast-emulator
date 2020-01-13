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

struct PowerVR_impl : PowerVR {

    void Reset(bool Manual)
    {

    }

    s32 Init()
    {
        rend_init_renderer();

        return rv_ok;
    }

    //called when exiting from sh4 thread , from the new thread context (for any thread specific de init) :P
    void Term()
    {
        rend_term_renderer();
    }
};

PowerVR* PowerVR::Create() {
    return new PowerVR_impl();
}