/*
	Highly inefficient and boring interpreter. Nothing special here
*/

#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_rom.h"
#include "../sh4_if.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/aica/aica_mmio.h"
#include "../modules/dmac.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_if.h"
#include "../sh4_interrupts.h"
#include "../modules/tmu.h"
#include "hw/sh4/sh4_mem.h"
#include "../modules/ccn.h"
#include "profiler/profiler.h"
#include "../dyna/blockmanager.h"
#include "../sh4_sched.h"
#include "hw/arm7/SoundCPU.h"

#include "libswirl.h"

#include <time.h>
#include <float.h>

#define CPU_RATIO      (8)

//uh uh
#define GetN(str) ((str>>8) & 0xf)
#define GetM(str) ((str>>4) & 0xf)

static s32 l;
//General update

//3584 Cycles
#define AICA_SAMPLE_GCM 441
#define AICA_SAMPLE_CYCLES (SH4_MAIN_CLOCK/(44100/AICA_SAMPLE_GCM)*32)

int aica_schid = -1;
int ds_schid = -1;
//14336 Cycles

const int AICA_TICK = 145124;


static void ExecuteOpcode(u16 op)
{
	if (sr.FD == 1 && OpDesc[op]->IsFloatingPoint())
		RaiseFPUDisableException();
	OpPtr[op](op);
	l -= CPU_RATIO;
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

int AicaUpdate(void* psh4, int tag, int c, int j)
{
    //gpc_counter=0;
    //bm_Periodical_14k();

    //static int aica_sample_cycles=0;
    //aica_sample_cycles+=14336*AICA_SAMPLE_GCM;

    //if (aica_sample_cycles>=AICA_SAMPLE_CYCLES)
    {
        sh4_cpu->GetA0H<SoundCPU>(A0H_SCPU)->Update(512 * 32);
        sh4_cpu->GetA0H<AICA>(A0H_AICA)->Update(1 * 32);
        //aica_sample_cycles-=AICA_SAMPLE_CYCLES;
    }

    return AICA_TICK;
}


int DreamcastSecond(void* psh4, int tag, int c, int j)
{
#if 1 //HOST_OS==OS_WINDOWS
    prof_periodical();
#endif

#if FEAT_SHREC != DYNAREC_NONE
    bm_Periodical_1s();
#endif

    //printf("%d ticks\n",sh4_sched_intr);
    sh4_sched_intr = 0;
    return SH4_MAIN_CLOCK;
}

// every SH4_TIMESLICE cycles
int UpdateSystem()
{
    //this is an optimisation (mostly for ARM)
    //makes scheduling easier !
    //update_fp* tmu=pUpdateTMU;

    Sh4cntx.sh4_sched_next -= SH4_TIMESLICE;
    if (Sh4cntx.sh4_sched_next < 0)
        sh4_sched_tick(SH4_TIMESLICE);

    return Sh4cntx.interrupt_pend;
}

int UpdateSystem_INTC()
{
    if (UpdateSystem())
        return UpdateINTC();
    else
        return 0;
}

struct SH4IInterpreter : SuperH4Backend {

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
                } while (l > 0);
                l += SH4_TIMESLICE;
                UpdateSystem_INTC();
#if !defined(NO_MMU)
            }
            catch (SH4ThrownException& ex) {
                Do_Exception(ex.epc, ex.expEvn, ex.callVect);
                l -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
            }
#endif

        } while (sh4_int_bCpuRun);
    }

    bool Init() { return true; }
    void Term() { }
    void ClearCache() { }

};

SuperH4Backend* Get_Sh4Interpreter()
{
    return new SH4IInterpreter();
}

#include "../SuperH4_impl.h"
/// SuperH4_impl



void SuperH4_impl::SetA0Handler(Area0Hanlders slot, MMIODevice* dev) {
    devices[slot].reset(dev);
}

MMIODevice* SuperH4_impl::GetA0Handler(Area0Hanlders slot) {
    return devices[slot].get();
}

bool SuperH4_impl::setBackend(SuperH4Backends backend) {
    if (sh4_backend) { sh4_backend->Term(); delete sh4_backend; sh4_backend = nullptr; }

    switch (backend)
    {
    case SH4BE_INTERPRETER:
        sh4_backend = Get_Sh4Interpreter();
        break;

    case SH4BE_DYNAREC:
        sh4_backend = Get_Sh4Recompiler();
        break;

    default:
        return false;

    }
    return sh4_backend->Init();
}

void SuperH4_impl::Run() {
    sh4_int_bCpuRun = true;

    sh4_backend->Loop();

    sh4_int_bCpuRun = false;
}

void SuperH4_impl::Stop()
{
    if (sh4_int_bCpuRun)
    {
        sh4_int_bCpuRun = false;
    }
}

void SuperH4_impl::Start()
{
    if (!sh4_int_bCpuRun)
    {
        sh4_int_bCpuRun = true;
    }
}

void SuperH4_impl::Step()
{
    if (sh4_int_bCpuRun)
    {
        printf("Sh4 Is running , can't step\n");
    }
    else
    {
        u32 op = ReadMem16(next_pc);
        next_pc += 2;
        ExecuteOpcode(op);
    }
}

void SuperH4_impl::Skip()
{
    if (sh4_int_bCpuRun)
    {
        printf("Sh4 Is running, can't Skip\n");
    }
    else
    {
        next_pc += 2;
    }
}

void SuperH4_impl::Reset(bool Manual)
{
    if (sh4_int_bCpuRun)
    {
        printf("Sh4 Is running, can't Reset\n");
    }
    else
    {
        next_pc = 0xA0000000;

        memset(r, 0, sizeof(r));
        memset(r_bank, 0, sizeof(r_bank));

        gbr = ssr = spc = sgr = dbr = vbr = 0;
        mac.full = pr = fpul = 0;

        sh4_sr_SetFull(0x700000F0);
        old_sr.status = sr.status;
        UpdateSR();

        fpscr.full = 0x0004001;
        old_fpscr = fpscr;
        UpdateFPSCR();

        //Any more registers have default value ?
        printf("Sh4 Reset\n");

        //Clear cache
        sh4_backend->ClearCache();
    }
}

bool SuperH4_impl::IsRunning()
{
    return sh4_int_bCpuRun;
}

bool SuperH4_impl::Init()
{
    verify(sizeof(Sh4cntx) == 448);

    if (aica_schid == -1)
    {
        aica_schid = sh4_sched_register(sh4_cpu, 0, &AicaUpdate);
        sh4_sched_request(aica_schid, AICA_TICK);

        ds_schid = sh4_sched_register(sh4_cpu, 0, &DreamcastSecond);
        sh4_sched_request(ds_schid, SH4_MAIN_CLOCK);
    }
    memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));

    setBackend(SH4BE_INTERPRETER);

    return true;
}

void SuperH4_impl::Term()
{
    Stop();
    printf("Sh4 Term\n");
}

void SuperH4_impl::ResetCache() {
    sh4_backend->ClearCache();
}