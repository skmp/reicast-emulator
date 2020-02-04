/*
	This file is part of libswirl
*/
#include "license/bsd"


/*
	Highly inefficient and boring interpreter. Nothing special here
*/

#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_interrupts.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "hw/sh4/sh4_mem.h"


#define CPU_RATIO      (8)

//uh uh
#define GetN(str) ((str>>8) & 0xf)
#define GetM(str) ((str>>4) & 0xf)

void ExecuteOpcode(u16 op)
{
	if (sr.FD == 1 && OpDesc[op]->IsFloatingPoint())
		RaiseFPUDisableException();
	OpPtr[op](op);
}

//TODO : Check for valid delayslot instruction
void ExecuteDelayslot()
{
#if !defined(NO_MMU)
    try {
#endif
        u32 addr = next_pc;
        next_pc += 2;
        u32 op = IReadMem16(addr);

        if (op != 0)	// Looney Tunes: Space Race hack
            ExecuteOpcode(op);
#if !defined(NO_MMU)
    }
    catch (SH4ThrownException& ex) {
        ex.epc -= 2;
        if (ex.expEvn == 0x800)	// FPU disable exception
            ex.expEvn = 0x820;	// Slot FPU disable exception
        else if (ex.expEvn == 0x180)	// Illegal instruction exception
            ex.expEvn = 0x1A0;			// Slot illegal instruction exception
        //printf("Delay slot exception\n");
        throw ex;
    }
#endif
}

void ExecuteDelayslot_RTE()
{
#if !defined(NO_MMU)
    try {
#endif
        ExecuteDelayslot();
#if !defined(NO_MMU)
    }
    catch (SH4ThrownException& ex) {
        msgboxf("Exception in RTE delay slot", MBX_ICONERROR);
    }
#endif
}

struct SH4IInterpreter : SuperH4Backend {
    s32 l;

    ~SH4IInterpreter() { Term(); }
    void Loop()
    {
        l = SH4_TIMESLICE;

        do
        {
#if !defined(NO_MMU)
            try {
#endif
                do
                {
                    u32 addr = next_pc;
                    next_pc += 2;
                    u32 op = IReadMem16(addr);

                    ExecuteOpcode(op);

                    l -= CPU_RATIO;
                } while (l > 0);

                l += SH4_TIMESLICE;

                // model how the jit works
                // only check for sh4_int_bCpuRun on interrupts
                if (UpdateSystem()) {
                    UpdateINTC();
                    if (!sh4_int_bCpuRun)
                        break;
                }

#if !defined(NO_MMU)
            }
            catch (SH4ThrownException& ex) {
                Do_Exception(ex.epc, ex.expEvn, ex.callVect);
                l -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
            }
#endif
        } while (true);
    }

    bool Init() { return true; }
    void Term() { }
    void ClearCache() { }

};

SuperH4Backend* Get_Sh4Interpreter()
{
    return new SH4IInterpreter();
}