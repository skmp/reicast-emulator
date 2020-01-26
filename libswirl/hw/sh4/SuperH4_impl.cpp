#include "SuperH4_impl.h"
#include "sh4_interpreter.h"
#include "sh4_core.h"
#include "sh4_mmio.h"
#include "sh4_mem.h"
#include "sh4_sched.h"
#include "sh4_interrupts.h"

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


/// SuperH4_impl



void SuperH4_impl::SetA0Handler(Area0Hanlders slot, MMIODevice* dev) {
    devices[slot].reset(dev);
}

MMIODevice* SuperH4_impl::GetA0Handler(Area0Hanlders slot) {
    return devices[slot].get();
}

bool SuperH4_impl::setBackend(SuperH4Backends backend) {
    switch (backend)
    {
    case SH4BE_INTERPRETER:
        sh4_backend.reset(Get_Sh4Interpreter());
        break;

    case SH4BE_DYNAREC:
        sh4_backend.reset(Get_Sh4Recompiler());
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

        sh4mmr->Reset();
    }
}

bool SuperH4_impl::IsRunning()
{
    return sh4_int_bCpuRun;
}

SuperH4_impl::SuperH4_impl() {

}

bool SuperH4_impl::Init()
{
    verify(sizeof(Sh4cntx) == 448);

    memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));

    setBackend(SH4BE_INTERPRETER);

    return true;
}

void SuperH4_impl::Term()
{
    sh4mmr.reset();

    Stop();

    sh4_sched_cleanup();

    sh4_backend.reset();

    printf("Sh4 Term\n");
}

void SuperH4_impl::ResetCache() {
    sh4_backend->ClearCache();
}

void SuperH4_impl::serialize(void** data, unsigned int* total_size) {
    for (int i = 0; i < A0H_MAX; i++) {
        sh4_cpu->GetA0Handler((Area0Hanlders)i)->serialize(data, total_size);
    }

    sh4mmr->serialize(data, total_size);
}

void SuperH4_impl::unserialize(void** data, unsigned int* total_size) {
    for (int i = 0; i < A0H_MAX; i++) {
        sh4_cpu->GetA0Handler((Area0Hanlders)i)->unserialize(data, total_size);
    }

    sh4mmr->unserialize(data, total_size);
}

SuperH4* SuperH4::Create() {

    auto rv = new SuperH4_impl();

    return rv;
}