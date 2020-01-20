#include "types.h"

#if HOST_CPU == CPU_ARM && FEAT_AREC == DYNAREC_JIT

#if HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN
#include <sys/mman.h>
#endif

#if HOST_OS == OS_WINDOWS
#include <Windows.h>
#endif


#include "arm7.h"
#include "arm7_context.h"
#include "arm7_jit_virt_backend.h"

#include "virt_arm.h"

#include "deps/vixl/aarch32/macro-assembler-aarch32.h"


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



using namespace vixl::aarch32;


#define arm_reg ctx->regs
#define armMode ctx->armMode

#define armNextPC arm_reg[R15_ARM_NEXT].I
#define CPUReadMemoryQuick(addr) (*(u32*)&ctx->aica_ram[addr&ctx->aram_mask])

#if HOST_OS==OS_DARWIN
#include <libkern/OSCacheControl.h>
extern "C" void armFlushICache(void* code, void* pEnd) {
    sys_dcache_flush(code, (u8*)pEnd - (u8*)code + 1);
    sys_icache_invalidate(code, (u8*)pEnd - (u8*)code + 1);
}
#else
extern "C" void armFlushICache(void* bgn, void* end) {
    __clear_cache(bgn, end);
}
#endif

/* arm backend
 * based on the virt-x86 one
 * shares lots of code with it
*/
struct Arm7VirtBackendArm32 : Arm7VirtBackend {
    MacroAssembler* assembler;
    ARM7Backend* arm;
    Arm7Context* ctx;

    u8* ICache;
    u8* icPtr_Base;
    u8* icPtr;

    Arm7VirtBackendArm32(ARM7Backend* arm, Arm7Context* ctx) : arm(arm), ctx(ctx) {

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

        printf("\n\t ARM7_TCB addr: %p | from: %p | addr here: %p\n", ICache, ARM7_TCB, &ARM7Backend::singleOp);

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
#if HOST_OS == OS_DARWIN
        return (ARM::eReg)11;
#else
        return (ARM::eReg)9;
#endif
    }

    void GenerateLooppoints(Looppoints* lp) {
        void* codestart = icPtr;
        assembler = new MacroAssembler(icPtr, ICache + ICacheSize - icPtr);

        // generate the loop points here

        lp->dispatch = assembler->GetCursorAddress<void*>();
        // arm_dispatch
        {

  /*
                ldrd r0, r1, [r8, #184]		@load : Next PC, interrupt

                @ TODO : FIX THIS TO NOT BE STATIC / CODEGEN on INIT
#if INTERNAL_ARAM_SIZE == 2*1024*1024
                ubfx r2, r0, #2, #19		@ assuming 2 MB address space max(21 bits)
#elif INTERNAL_ARAM_SIZE == 8*1024*1024
                ubfx r2, r0, #2, #21		@ assuming 8 MB address space max(23 bits)
#else
#error Unsupported AICA RAM size
#endif
*/
            assembler->Ldrd(r0, r1, MemOperand(r8, 184));
            if ((ctx->aram_mask + 1) == 2 * 1024 * 1024) {
                assembler->Ubfx(r2, r0, 2, 19);
            }
            else if ((ctx->aram_mask + 1) == 8 * 1024 * 1024) {
                assembler->Ubfx(r2, r0, 2, 21);
            }
            else {
                die("Unsupported AICA RAM size");
            }

            /*
cmp r1, #0
bne arm_dofiq

ldr pc, [r4, r2, lsl #2]
*/
            Label arm_dofiq;
            assembler->Cbnz(r1, &arm_dofiq);
            assembler->Add(r2, r4, Operand(r2, LSL, 3));
            assembler->Ldr(pc, MemOperand(r2));


            /*
                arm_dofiq:
                bl CSYM(CPUFiq)
                b CSYM(arm_dispatch)
            */
            assembler->Bind(&arm_dofiq);
            assembler->Mov(r0, (uintptr_t)ctx);
            ptrdiff_t offset = reinterpret_cast<uintptr_t>(&ARM7Backend::CPUFiq) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label CPUFiq_label;
            assembler->BindToOffset(&CPUFiq_label, offset);
            assembler->Bl(&CPUFiq_label);
            offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label arm_dispatch_label;
            assembler->BindToOffset(&arm_dispatch_label, offset);
            assembler->B(&arm_dispatch_label);
        }

        lp->mainloop = assembler->GetCursorAddress<void*>();
        // arm_mainloop
        {
 

                 /*
                 #if HOST_OS == OS_DARWIN
                                 push {
                                 r4, r5, r8, r11, lr
                             }
                 #else
                                 push{ r4,r5,r8,r9,lr }
                 #endif
 */
            assembler->Push(RegisterList(r4, r5, r8, lr));
            assembler->Push(RegisterList(Register(GetSafeReg())));

            /*
                #ifdef TARGET_IPHONE
                                ldr r8, Xarm_Reg			@load cntx
                                ldr r4, XEntryPoints		@load lookup base
                #else
                                mov r8, r1			@load cntx
                                mov r4, r2		@load lookup base
                #endif

                                ldr r5, [r8, #192]	@load cycle count
                                add r5, r0			@add cycles for this timeslice

                 */

            assembler->Mov(r8, (uintptr_t)ctx);
            assembler->Mov(r4, (uintptr_t)arm->GetEntrypointBase());
            
            assembler->Ldr(r5, MemOperand(r8, 192));
            assembler->Add(r5, r5, r0);

            //b CSYM(arm_dispatch)
            ptrdiff_t offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label arm_dispatch_label;
            assembler->BindToOffset(&arm_dispatch_label, offset);
            assembler->B(&arm_dispatch_label);
        }

        lp->compilecode = assembler->GetCursorAddress<void*>();
        // arm_compilecode
        {
            assembler->Mov(r0, (uintptr_t)arm);
            ptrdiff_t offset = reinterpret_cast<uintptr_t>(&CompileCode) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label CompileCode_label;
            assembler->BindToOffset(&CompileCode_label, offset);
            assembler->Bl(&CompileCode_label);
            offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label arm_dispatch_label;
            assembler->BindToOffset(&arm_dispatch_label, offset);
            assembler->B(&arm_dispatch_label);
        }

        lp->exit = assembler->GetCursorAddress<void*>();
        // arm_exit
        {
        /*
            CSYM(arm_exit) :
                str r5, [r8, #192]		@if timeslice is over, save remaining cycles
#if HOST_OS == OS_DARWIN
                pop{ r4,r5,r8,r11,pc }
#else
                pop{ r4,r5,r8,r9,pc }
#endif
        */
            //assembler->Brk(0);
            assembler->Str(r5, MemOperand(r8, 192));
            assembler->Pop(RegisterList(Register(GetSafeReg())));
            assembler->Pop(RegisterList(r4, r5, r8, pc));
        }


        // cleanup

        assembler->FinalizeCode();
        verify(assembler->GetBuffer()->GetCursorOffset() <= assembler->GetBuffer()->GetCapacity());
        armFlushICache(codestart, assembler->GetBuffer()->GetEndAddress<void*>());

        icPtr += assembler->GetBuffer()->GetSizeInBytes();
        icPtr_Base = icPtr;

        delete assembler;
        assembler = nullptr;
    }

    MemOperand arm_reg_operand(u32 regn)
    {
        return MemOperand(r8, (u8*)&ctx->regs[regn].I - (u8*)&ctx->regs[0].I);
    }

    void* armGetEmitPtr()
    {
        if (icPtr < (ICache + ICacheSize - 1024))	//ifdebug
            return static_cast<void*>(icPtr);

        return NULL;
    }

    //helpers ...
    void LoadReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL)
    {
        assembler->Ldr(Register(rd), arm_reg_operand(regn));
    }

    void StoreReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL)
    {
        assembler->Str(Register(rd), arm_reg_operand(regn));
    }

    void* start_conditional(ARM::ConditionCode cc)
    {
        if (cc == ARM::CC_AL)
            return NULL;
        Label* label = new Label();
        verify(cc <= ARM::CC_LE);
        Condition condition = (Condition)((u32)cc ^ 1);
        assembler->B(condition, label);

        return label;
    }

    void end_conditional(void* ref)
    {
        if (ref != NULL)
        {
            Label* label = (Label*)ref;
            assembler->Bind(label);
            delete label;
        }
    }

    // FIXME IMPL
    //For COND
    void LoadFlags()
    {
        //Load flags
        LoadReg(ARM::r0, RN_PSR_FLAGS);
        //move them to flags register
        assembler->Msr(MaskedSpecialRegister(APSR_nzcvq), r0);
    }

    // FIXME IMPL
    void StoreFlags()
    {
        //get results from flags register
        assembler->Mrs(r1, SpecialRegister(APSR_nzcvq));
        //Store flags
        StoreReg(ARM::r1, RN_PSR_FLAGS);
    }

    void imm_to_reg(u32 regn, u32 imm)
    {
        assembler->Mov(r0, imm);
        assembler->Str(r0, arm_reg_operand(regn));
    }

    void call(void* loc, int params, int returns)
    {
        ptrdiff_t offset = reinterpret_cast<uintptr_t>(loc) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
        Label function_label;
        assembler->BindToOffset(&function_label, offset);
        assembler->Bl(&function_label);
    }

    
    bool setup()
    {
        if (icPtr >= (ICache + ICacheSize - 64 * 1024)) {
            return false;
        }

        assembler = new MacroAssembler(icPtr, ICache + ICacheSize - icPtr);

        return true;
    }

    void intpr(u32 opcd)
    {
        //Call interpreter
        assembler->Mov(r0, (uintptr_t)ctx);
        assembler->Mov(r1, opcd);
        call((void*)&ARM7Backend::singleOp, 1, 1);
    }


    void end(Looppoints* lp, void* codestart, u32 cycl)
    {
        //Normal block end
        //cycle counter rv
        
        //pop registers & return
        assembler->Subs(r5, r5, cycl);
        ptrdiff_t offset = reinterpret_cast<uintptr_t>(lp->exit) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
        Label arm_exit_label;
        assembler->BindToOffset(&arm_exit_label, offset);
        assembler->B(mi, &arm_exit_label);	//statically predicted as not taken

        offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
        Label arm_dispatch_label;
        assembler->BindToOffset(&arm_dispatch_label, offset);
        assembler->B(&arm_dispatch_label);

        assembler->FinalizeCode();
        verify(assembler->GetBuffer()->GetCursorOffset() <= assembler->GetBuffer()->GetCapacity());
        armFlushICache(codestart, assembler->GetBuffer()->GetEndAddress<void*>());

        icPtr += assembler->GetBuffer()->GetSizeInBytes();

#if 0
        Instruction* instr_start = (Instruction*)codestart;
        Instruction* instr_end = assembler->GetBuffer()->GetEndAddress<Instruction*>();
        Decoder decoder;
        Disassembler disasm;
        decoder.AppendVisitor(&disasm);
        Instruction* instr;
        for (instr = instr_start; instr < instr_end; instr += kInstructionSize) {
            decoder.Decode(instr);
            printf("arm64 arec\t %p:\t%s\n",
                reinterpret_cast<void*>(instr),
                disasm.GetOutput());
        }
#endif
        delete assembler;
        assembler = NULL;
    }

    //Hook cus varm misses this, so x86 needs special code
    void MOVPTR(ARM::eReg regn, uintptr_t imm)
    {
        assembler->Mov(Register(regn), imm);
    }

    void MOV32(ARM::eReg regn, u32 imm)
    {
        assembler->Mov(Register(regn), imm);
    }


    void mov(ARM::eReg regd, ARM::eReg regn)
    {
        assembler->Mov(Register(regd), Register(regn));
    }

    virtual void sxtb(ARM::eReg regd, ARM::eReg regs)
    {
        assembler->Sxtb(Register(regd), Register(regs));
    }

    virtual void zxtb(ARM::eReg regd, ARM::eReg regs)
    {
        assembler->And(Register(regd), Register(regs), 0xFF);
    }


    void add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        assembler->Add(Register(regd), Register(regn), Register(regm));
    }

    void sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        assembler->Sub(Register(regd), Register(regn), Register(regm));
    }

    void add(ARM::eReg regd, ARM::eReg regn, s32 imm)
    {
        assembler->Add(Register(regd), Register(regn), imm);
    }

    void lsl(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        assembler->Lsl(Register(regd), Register(regn), imm);
    }

    void bic(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        assembler->Bic(Register(regd), Register(regn), imm);
    }

    void Emit32(u32 emit32)
    {
        assembler->EmitA32(emit32);
    }

    //sanity check: non branch doesn't set pc
    void check_pc(u32 pc)
    {
    
    }

    //sanity check: stale cache
    void check_cache(u32 opcd, u32 pc)
    {
 
    }

    //profiler hook
    void prof(OpType opt, u32 op, u32 flags)
    {
    
    }

    void InvalidateJitCache() {
        icPtr = icPtr_Base;
    }
};

Arm7VirtBackend* Arm7VirtBackend::Create(ARM7Backend* arm, Arm7Context* ctx) {
    return new Arm7VirtBackendArm32(arm, ctx);
}
#endif