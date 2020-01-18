#include "types.h"

#if HOST_CPU == CPU_X86 && FEAT_AREC == DYNAREC_JIT

#if HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN
#include <sys/mman.h>
#endif
#if HOST_OS == OS_WINDOWS
#include <Windows.h>
#endif


void  armEmit32(u32 emit32);
void* armGetEmitPtr();


#define _DEVEL          (1)
#define EMIT_I          armEmit32((I))
#define EMIT_GET_PTR()  armGetEmitPtr()

#include "jit/emitter/arm32/arm_emitter.h"

#include "arm7.h"
#include "arm7_context.h"
#include "arm7_jit_virt_backend.h"

#include "virt_arm.h"


#include "jit/emitter/x86/x86_emitter.h"

u8* ARM::emit_opt = 0;
ARM::eReg ARM::reg_addr;
ARM::eReg ARM::reg_dst;
s32 ARM::imma;


const u32 ICacheSize = 1024 * 1024;
#if HOST_OS == OS_WINDOWS
u8 ARM7_TCB[ICacheSize + 4096];
#elif HOST_OS == OS_LINUX

u8 ARM7_TCB[ICacheSize + 4096] __attribute__((section(".text")));

#elif HOST_OS==OS_DARWIN
u8 ARM7_TCB[ICacheSize + 4096] __attribute__((section("__TEXT, .text")));
#else
#error ARM7_TCB ALLOC
#endif



using namespace ARM;


#define arm_reg ctx->regs
#define armMode ctx->armMode

#define armNextPC arm_reg[R15_ARM_NEXT].I
#define CPUReadMemoryQuick(addr) (*(u32*)&ctx->aica_ram[addr&ctx->aram_mask])

//profiler
u32 nfb, ffb, bfb, mfb;

/* X86 backend
 * Uses a mix of
 * x86 code
 * Virtualised arm code (using the varm interpreter)
 * Emulated arm fallbacks (using the aica arm interpreter)
 *
 * The goal is to run as much code possible under the varm interpreter
 * so it will run on arm w/o changes. A few opcodes are missing from varm
 * (MOV32 is a notable case) and as such i've added a few varm_* hooks
 *
 * This code also performs a LOT of compiletime and runtime state/value sanity checks.
 * We don't care for speed here ...
*/
struct Arm7VirtBackendX86 : Arm7VirtBackend {
    x86_block* x86e;
    ARM7Backend* arm;
    Arm7Context* ctx;

    u8* ICache;
    u8* icPtr_Base;
    u8* icPtr;

    Arm7VirtBackendX86(ARM7Backend* arm, Arm7Context* ctx) : arm(arm), ctx(ctx) {

        //align to next page ..
        ICache = (u8*)(((unat)ARM7_TCB + 4095) & ~4095);

#if HOST_OS==OS_DARWIN
        //Can't just mprotect on iOS
        munmap(ICache, ICacheSize);
        ICache = (u8*)mmap(ICache, ICacheSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE | MAP_ANON, 0, 0);
#endif

#if HOST_OS == OS_WINDOWS
        DWORD old;
        VirtualProtect(ICache, ICacheSize, PAGE_EXECUTE_READWRITE, &old);
#elif HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN

        printf("\n\t ARM7_TCB addr: %p | from: %p | addr here: %p\n", ICache, ARM7_TCB, armt_init);

        if (mprotect(ICache, ICacheSize, PROT_EXEC | PROT_READ | PROT_WRITE))
        {
            perror("\n\tError - Couldn�t mprotect ARM7_TCB!");
            verify(false);
        }

#if TARGET_IPHONE
        memset((u8*)mmap(ICache, ICacheSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE | MAP_ANON, 0, 0), 0xFF, ICacheSize);
#else
        memset(ICache, 0xFF, ICacheSize);
#endif

#endif

        icPtr = ICache;
    }

    ARM::eReg GetSafeReg() {
        return r3;
    }

    void GenerateLooppoints(Looppoints* lp) {
        x86e = new x86_block();

        x86e->Init(0, 0);
        x86e->x86_buff = (u8*)EMIT_GET_PTR();
        x86e->x86_size = 1024 * 64;
        x86e->do_realloc = false;

        lp->dispatch = &x86e->x86_buff[x86e->x86_indx];
        // arm_dispatch
        {
            x86e->Emit(op_mov32, EAX, &arm_reg[R15_ARM_NEXT].I);
            x86e->Emit(op_and32, EAX, ctx->aram_mask);
            x86e->Emit(op_cmp32, &arm_reg[INTR_PEND].I, 0);

            x86_Label* dofiq = x86e->CreateLabel(false, 8);
            x86e->Emit(op_jne, dofiq);
            x86e->Emit(op_jmp32, x86_mrm(EAX, arm->GetEntrypointBase()));


            x86e->MarkLabel(dofiq);
            x86e->Emit(op_mov32, ECX, (uintptr_t)ctx);
            x86e->Emit(op_call, x86_ptr_imm(ARM7Backend::CPUFiq));
            x86e->Emit(op_jmp, x86_ptr_imm(lp->dispatch));
        }

        lp->mainloop = &x86e->x86_buff[x86e->x86_indx];
        // arm_mainloop
        {
            x86e->Emit(op_push32, ESI);
            x86e->Emit(op_mov32, ESI, ECX);
            x86e->Emit(op_add32, ESI, &arm_reg[CYCL_CNT].I);

            x86e->Emit(op_mov32, EAX, 0);
            x86e->Emit(op_jmp, x86_ptr_imm(lp->dispatch));
        }

        lp->compilecode = &x86e->x86_buff[x86e->x86_indx];
        // arm_compilecode
        {
            x86e->Emit(op_mov32, ECX, (uintptr_t)arm);
            x86e->Emit(op_call, x86_ptr_imm(CompileCode));
            x86e->Emit(op_mov32, EAX, 0);
            x86e->Emit(op_jmp, x86_ptr_imm(lp->dispatch));
        }

        lp->exit = &x86e->x86_buff[x86e->x86_indx];
        // arm_exit
        {
            x86e->Emit(op_mov32, &arm_reg[CYCL_CNT].I, ESI);
            x86e->Emit(op_pop32, ESI);
            x86e->Emit(op_ret);
        }

        //Generate the code & apply fixups/relocations as needed
        x86e->Generate();

        //Use space from the dynarec buffer
        icPtr += x86e->x86_indx;

        // Set the reset position
        icPtr_Base = icPtr;

        //Delete the x86 emitter ...
        delete x86e;
    }

    void* armGetEmitPtr()
    {
        if (icPtr < (ICache + ICacheSize - 1024))	//ifdebug
            return static_cast<void*>(icPtr);

        return NULL;
    }

    void LoadReg(eReg rd, u32 regn, ConditionCode cc = CC_AL)
    {
        LDR(rd, r8, (u8*)&arm_reg[regn].I - (u8*)&arm_reg[0].I, Offset, cc);
    }
    void StoreReg(eReg rd, u32 regn, ConditionCode cc = CC_AL)
    {
        STR(rd, r8, (u8*)&arm_reg[regn].I - (u8*)&arm_reg[0].I, Offset, cc);
    }

    void LoadFlags()
    {
        //Load flags
        LoadReg(r0, RN_PSR_FLAGS);
        //move them to flags register
        MSR(0, 8, r0);
    }

    void StoreFlags()
    {
        //get results from flags register
        MRS(r1, 0);
        //Store flags
        StoreReg(r1, RN_PSR_FLAGS);
    }

    void mov(ARM::eReg regd, ARM::eReg regn)
    {
        MOV(regd, regn);
    }

    void sxtb(ARM::eReg regd, ARM::eReg regs)
    {
        x86e->Emit(op_movsx8to32, EAX, &virt_arm_reg(regs));
        x86e->Emit(op_mov32, &virt_arm_reg(regd), EAX);
    }

    void zxtb(ARM::eReg regd, ARM::eReg regs)
    {
        x86e->Emit(op_movzx8to32, EAX, &virt_arm_reg(regs));
        x86e->Emit(op_mov32, &virt_arm_reg(regd), EAX);
    }

    void add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        ADD(regd, regn, regm);
    }

    void sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        SUB(regd, regn, regm);
    }

    void add(ARM::eReg regd, ARM::eReg regn, s32 imm)
    {
        if (imm >= 0)
            ADD(regd, regn, imm);
        else
            SUB(regd, regn, -imm);
    }

    void lsl(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        LSL(regd, regn, imm);
    }

    void bic(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        BIC(regd, regn, imm);
    }

    void* start_conditional(ARM::ConditionCode cc)
    {
        return NULL;
    }
    void end_conditional(void* ref)
    {
    }


    void imm_to_reg(u32 regn, u32 imm)
    {
        x86e->Emit(op_mov32, &arm_reg[regn].I, imm);
    }

    void MOVPTR(eReg regn, uintptr_t imm)
    {
        x86e->Emit(op_mov32, &virt_arm_reg(regn), imm);
    }

    void MOV32(eReg regn, u32 imm)
    {
        x86e->Emit(op_mov32, &virt_arm_reg(regn), imm);
    }

    void call(void* loc, int params, int returns)
    {
        verify(params <= 3);

        if (params) {
            if (params >= 1) {
                x86e->Emit(op_mov32, ECX, &virt_arm_reg(0));
            }
            if (params >= 2) {
                x86e->Emit(op_mov32, EDX, &virt_arm_reg(1));
            }
            if (params >= 3) {
                x86e->Emit(op_push32, x86_ptr(&virt_arm_reg(2)));
            }

        }

        x86e->Emit(op_call, x86_ptr_imm(loc));

        if (returns) {
            x86e->Emit(op_mov32, &virt_arm_reg(0), EAX);
        }
    }

    x86_Label* end_lbl;

    bool setup()
    {
        if (icPtr >= (ICache + ICacheSize - 64 * 1024)) {
            return false;
        }

        //Setup emitter
        x86e = new x86_block();
        x86e->Init(0, 0);
        x86e->x86_buff = (u8*)EMIT_GET_PTR();

        verify(x86e->x86_buff != nullptr);

        x86e->x86_size = 1024 * 64;
        x86e->do_realloc = false;


        //load base reg ..
        x86e->Emit(op_mov32, &virt_arm_reg(8), (u32)&arm_reg[0]);

        //the "end" label is used to exit from the block, if a code modification (expected opcode // actual opcode in ram) is detected
        end_lbl = x86e->CreateLabel(false, 0);

        return true;
    }

    void intpr(u32 opcd)
    {
        //Call interpreter
        x86e->Emit(op_mov32, ECX, (uintptr_t)ctx);
        x86e->Emit(op_mov32, EDX, opcd);
        x86e->Emit(op_call, x86_ptr_imm(&ARM7Backend::singleOp));
    }

    void end(Looppoints* lp, void* codestart, u32 cycles)
    {
        //Normal block end
        //Move counter to EAX for return, pop ESI, ret
        x86e->Emit(op_sub32, ESI, cycles);
        x86e->Emit(op_jns, x86_ptr_imm(lp->dispatch));
        x86e->Emit(op_jmp, x86_ptr_imm(lp->exit));

        //Fluch cache, move counter to EAX, pop, ret
        //this should never happen (triggers a breakpoint on x86)
        x86e->MarkLabel(end_lbl);
        x86e->Emit(op_int3);
        x86e->Emit(op_int3);
        //x86e->Emit(op_call, x86_ptr_imm(FlushCache));
        x86e->Emit(op_sub32, ESI, cycles);
        x86e->Emit(op_jmp, x86_ptr_imm(lp->dispatch));

        //Generate the code & apply fixups/relocations as needed
        x86e->Generate();

        //Use space from the dynarec buffer
        icPtr += x86e->x86_indx;

        //Delete the x86 emitter ...
        delete x86e;
    }

    //sanity check: non branch doesn't set pc
    void check_pc(u32 pc)
    {
        x86e->Emit(op_cmp32, &armNextPC, pc);
        x86_Label* nof = x86e->CreateLabel(false, 0);
        x86e->Emit(op_je, nof);
        x86e->Emit(op_int3);
        x86e->Emit(op_int3);
        x86e->Emit(op_int3);
        x86e->MarkLabel(nof);
    }

    //sanity check: stale cache
    void check_cache(u32 opcd, u32 pc)
    {
        x86e->Emit(op_cmp32, &CPUReadMemoryQuick(pc), opcd);
        x86_Label* nof = x86e->CreateLabel(false, 0);
        x86e->Emit(op_je, nof);
        x86e->Emit(op_int3);
        x86e->MarkLabel(nof);
    }

    //profiler hook
    void prof(OpType opt, u32 op, u32 flags)
    {
        if (VOT_Fallback != opt)
            x86e->Emit(op_add32, &nfb, 1);
        else
        {
            if (flags & OP_SETS_PC)
                x86e->Emit(op_add32, &bfb, 1);
            else if (flags & OP_MFB)
                x86e->Emit(op_add32, &mfb, 1);
            else
                x86e->Emit(op_add32, &ffb, 1);
        }
    }

    void Emit32(u32 emit32)
    {
        if (icPtr >= (ICache + ICacheSize - 1024)) {
            die("ICache is full, invalidate old entries ...");	//ifdebug
        }

        x86e->Emit(op_mov32, ECX, emit32);
        x86e->Emit(op_call, x86_ptr_imm(virt_arm_op));
    }

    void InvalidateJitCache() {
        icPtr = icPtr_Base;
    }
};

Arm7VirtBackend* Arm7VirtBackend::Create(ARM7Backend* arm, Arm7Context* ctx) {
    return new Arm7VirtBackendX86(arm, ctx);
}
#endif