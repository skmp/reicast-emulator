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

    // Force an interrupt check if the cpu has been stopped
    // ngen is required to only check the bCpuRun on interrupt processing
    return Sh4cntx.interrupt_pend | (sh4_int_bCpuRun == false);
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
#if FEAT_SHREC != DYNAREC_NONE
    case SH4BE_DYNAREC:
        sh4_backend.reset(Get_Sh4Recompiler());
        break;
#endif

    default:
        return false;
    }

    return sh4_backend->Init();
}

void SuperH4_impl::Run() {
    sh4_backend->Loop();
}

void SuperH4_impl::Stop()
{
    verify(sh4_int_bCpuRun);

    sh4_int_bCpuRun = false;
}

void SuperH4_impl::Start()
{
    verify(!sh4_int_bCpuRun);
    
    sh4_int_bCpuRun = true;
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

        // reset mmrs/modules
        sh4mmr->Reset();

        // reset devices
        for (const auto& dev : devices)
            dev->Reset(Manual);
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

    // init devices
    for (const auto& dev : devices)
        if (!dev->Init())
            return false;

    return true;
}

void SuperH4_impl::Term()
{
    verify(!sh4_cpu->IsRunning());

    for (const auto& dev : devices)
        dev->Term();
    
    for (auto& dev : devices)
        dev.reset();
    
    sh4mmr.reset();

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