#include "arm7.h"
#include "arm_mem.h"
#include "virt_arm.h"
#include "arm7_context.h"
#include "hw/aica/aica_mmio.h"
#include <memory>

#define arm_printf(...)

#define arm_reg ctx->regs
#define armMode ctx->armMode

#define armNextPC arm_reg[R15_ARM_NEXT].I
#define CPUReadMemoryQuick(addr) (*(u32*)&ctx->aica_ram[addr&ctx->aram_mask])

static const u8 cpuBitsSet[256] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };

//findfirstset -- used in LDM/STM handling
#if HOST_CPU==CPU_X86 && BUILD_COMPILER != COMPILER_GCC
#include <intrin.h>

void  armEmit32(u32 emit32);
void* armGetEmitPtr();


#define _DEVEL          (1)
#define EMIT_I          armEmit32((I))
#define EMIT_GET_PTR()  armGetEmitPtr()

#include "jit/emitter/arm32/arm_emitter.h"

u8* ARM::emit_opt = 0;
ARM::eReg ARM::reg_addr;
ARM::eReg ARM::reg_dst;
s32 ARM::imma;

#include "jit/emitter/x86/x86_emitter.h"

u32 findfirstset(u32 v)
{
	unsigned long rv;
	_BitScanForward(&rv, v);
	return rv + 1;
}
#else
#define findfirstset __builtin_ffs
#endif


#if FEAT_AREC != DYNAREC_NONE

#if HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN
#include <sys/mman.h>
#endif
#if HOST_OS == OS_WINDOWS
#include <Windows.h>
#endif


/*
 *
 *	X86 Compiler
 *
 */


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


#undef I


using namespace ARM;


void* EntryPoints[ARAM_SIZE_MAX / 4];

enum OpType
{
	VOT_Fallback,
	VOT_DataOp,
	VOT_B,
	VOT_BL,
	VOT_BR,     //Branch (to register)
	VOT_Read,   //Actually, this handles LDR and STR
	//VOT_LDM,  //This Isn't used anymore
	VOT_MRS,
	VOT_MSR,
};



template<u32 Pd>
void DYNACALL MSR_do(Arm7Context* ctx, u32 v)
{
    if (Pd)
    {
        if (armMode > 0x10 && armMode < 0x1f) /* !=0x10 ?*/
        {
            arm_reg[17].I = (arm_reg[17].I & 0x00FFFF00) | (v & 0xFF0000FF);
        }
    }
    else
    {
        ARM7Backend::CPUUpdateCPSR(ctx);

        u32 newValue = arm_reg[16].I;
        if (armMode > 0x10)
        {
            newValue = (newValue & 0xFFFFFF00) | (v & 0x000000FF);
        }

        newValue = (newValue & 0x00FFFFFF) | (v & 0xFF000000);
        newValue |= 0x10;
        if (armMode > 0x10)
        {
            ARM7Backend::CPUSwitchMode(ctx, newValue & 0x1f, false);
        }
        arm_reg[16].I = newValue;
        ARM7Backend::CPUUpdateFlags(ctx);
    }
}


struct Looppoints
{
    void* compilecode;
    void* mainloop;
    void* dispatch;
    void* exit;
};

//profiler
u32 nfb, ffb, bfb, mfb;

// FIXME: arm7 decoder


    /*

        ARM
            ALU opcodes (more or less)

                (flags,rv)=opcode(flags,in regs ..)
                rd=rv;
                if (set_flags)
                    PSR=(rd==pc?CPSR:flags);

            (mem ops)
            Writes of R15:
                R15+12
            R15 as base:
                R15+8
            LDR
                rd=mem[addr(in regs)]
            LDM

            ...
            STR/STM: pc+12


            ///

            "cached" interpreter:
            Set PC+12 to PC reg
            mov opcode
            call function

            if (pc settting opcode)
                lookup again using armNextPC


            PC setting opcodes
                ALU with write to PC
                LDR with write to PC (SDT)
                LDM with write to PC (BDT)
                B/BL
                SWI
                <Undefined opcodes, if any>

                Indirect, via write to PSR/Mode
                MSR
    */


struct ArmDPOP
{
    u32 key;
    u32 mask;
    u32 flags;
};

vector<ArmDPOP> ops;

enum OpFlags
{
    OP_SETS_PC = 1,
    OP_READS_PC = 32768,
    OP_IS_COND = 65536,
    OP_MFB = 0x80000000,

    OP_HAS_RD_12 = 2,
    OP_HAS_RD_16 = 4,
    OP_HAS_RS_0 = 8,
    OP_HAS_RS_8 = 16,
    OP_HAS_RS_16 = 32,
    OP_HAS_FLAGS_READ = 4096,
    OP_HAS_FLAGS_WRITE = 8192,
    OP_HAS_RD_READ = 16384, //For conditionals

    OP_WRITE_FLAGS = 64,
    OP_WRITE_FLAGS_S = 128,
    OP_READ_FLAGS = 256,
    OP_READ_FLAGS_S = 512,
    OP_WRITE_REG = 1024,
    OP_READ_REG_1 = 2048,
};

#define DP_R_ROFC (OP_READ_FLAGS_S|OP_READ_REG_1) //Reads reg1, op2, flags if S
#define DP_R_ROF (OP_READ_FLAGS|OP_READ_REG_1)    //Reads reg1, op2, flags (ADC & co)
#define DP_R_OFC (OP_READ_FLAGS_S)                //Reads op2, flags if S

#define DP_W_RFC (OP_WRITE_FLAGS_S|OP_WRITE_REG)  //Writes reg, and flags if S
#define DP_W_F (OP_WRITE_FLAGS)                   //Writes only flags, always (S=1)

struct Arm7JitVirt_impl;
void DYNACALL CompileCode(Arm7JitVirt_impl* arm);

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
struct Arm7VirtBackend;

static Arm7VirtBackend* virtBackend;

struct Arm7VirtBackend {
    x86_block* x86e;
    ARM7Backend* arm;
    Arm7Context* ctx;

    u8* ICache;
    u8* icPtr_Base;
    u8* icPtr;

    Arm7VirtBackend(ARM7Backend* arm, Arm7Context* ctx) : arm(arm), ctx(ctx) {

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
                perror("\n\tError - Couldn’t mprotect ARM7_TCB!");
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
            x86e->Emit(op_and32, EAX, 0x7FFFFC);
            x86e->Emit(op_cmp32, &arm_reg[INTR_PEND].I, 0);

            x86_Label* dofiq = x86e->CreateLabel(false, 8);
            x86e->Emit(op_jne, dofiq);
            x86e->Emit(op_jmp32, x86_mrm(EAX, EntryPoints));


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

    void setup()
    {
        verify(virtBackend == nullptr);

        virtBackend = this;

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
    }

    void intpr(u32 opcd)
    {
        //Call interpreter
        x86e->Emit(op_mov32, ECX, ctx);
        x86e->Emit(op_mov32, EDX, opcd);
        x86e->Emit(op_call, x86_ptr_imm(&ARM7Backend::Step));
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

        verify(virtBackend == this);
        virtBackend = nullptr;
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
        if (icPtr >= (ICache + ICacheSize - 64 * 1024)) {
            die("ICache is full, invalidate old entries ...");	//ifdebug
            arm->InvalidateJitCache();
        }

        x86e->Emit(op_mov32, ECX, emit32);
        x86e->Emit(op_call, x86_ptr_imm(virt_arm_op));
    }

    void InvalidateJitCache() {
        icPtr = icPtr_Base;
    }
};

void  armEmit32(u32 emit32) {
    virtBackend->Emit32(emit32);
}
void* armGetEmitPtr() {
    return virtBackend->armGetEmitPtr();
}

struct Arm7JitVirt_impl : ARM7Backend {
    unique_ptr<Arm7VirtBackend> armv;
    Arm7Context* ctx;
    Looppoints lps;

    Arm7JitVirt_impl(Arm7Context* ctx) : ctx(ctx) {
        armv.reset(new Arm7VirtBackend(this, ctx));

        armv->GenerateLooppoints(&lps);

        armt_init();
    }

    void UpdateInterrupts()
    {
        ARM7Backend::UpdateInterrupts(ctx);
    }

    void Run(u32 CycleCount)
    {
        ((void (DYNACALL*)(u32))lps.mainloop)(CycleCount);
    }

    //Emulate a single arm op, passed in opcode
    //DYNACALL for ECX passing

    //FIXME: Move to diff file
    /*
        COND | 00 0 OP1   S Rn Rd SA    ST 0  Rm -- Data opcode, PSR xfer (imm shifted reg)
             | 00 0 OP1   S Rn Rd Rs   0 ST 1 Rm -- Data opcode, PSR xfer (reg shifted reg)
             | 00 0 0 00A S Rd Rn Rs    1001  Rm -- Mult
             | 00 0 1 0B0 0 Rn Rd 0000  1001  Rm -- SWP
             | 00 1 OP1   S Rn Rd imm8r4         -- Data opcode, PSR xfer (imm8r4)

             | 01 0 P UBW L Rn Rd Offset          -- LDR/STR (I=0)
             | 01 1 P UBW L Rn Rd SHAM SHTP 0 Rs  -- LDR/STR (I=1)
             | 10 0 P USW L Rn {RList}            -- LDM/STM
             | 10 1 L {offset}                    -- B/BL
             | 11 1 1 X*                          -- SWI

             (undef cases)
             | 01 1 XXXX X X*  X*  X* 1 XXXX - Undefined (LDR/STR w/ encodings that would be reg. based shift)
             | 11 0 PUNW L Rn {undef} -- Copr. Data xfer (undef)
             | 11 1 0 CPOP Crn Crd Cpn CP3 0 Crm -- Copr. Data Op (undef)
             | 11 1 0 CPO3 L Crn Crd Cpn CP3 1 Crm -- Copr. Reg xf (undef)


             Phase #1:
                -Non branches that don't touch memory (pretty much: Data processing, Not MSR, Mult)
                -Everything else is ifb

             Phase #2:
                Move LDR/STR to templates

             Phase #3:
                Move LDM/STM to templates


    */

    void AddDPOP(u32 subcd, u32 rflags, u32 wflags)
    {
        ArmDPOP op;

        u32 key = subcd << 21;
        u32 mask = (15 << 21) | (7 << 25);

        op.flags = rflags | wflags;

        if (wflags == DP_W_F)
        {
            //also match S bit for opcodes that must write to flags (CMP & co)
            mask |= 1 << 20;
            key |= 1 << 20;
        }

        //ISR form (bit 25=0, bit 4 = 0)
        op.key = key;
        op.mask = mask | (1 << 4);
        ops.push_back(op);

        //RSR form (bit 25=0, bit 4 = 1, bit 7=0)
        op.key = key | (1 << 4);
        op.mask = mask | (1 << 4) | (1 << 7);
        ops.push_back(op);

        //imm8r4 form (bit 25=1) 
        op.key = key | (1 << 25);
        op.mask = mask;
        ops.push_back(op);
    }

    void InitHash()
    {
        /*
            COND | 00 I OP1  S Rn Rd OPER2 -- Data opcode, PSR xfer
            Data processing opcodes
        */

        //AND   0000        Rn, OPER2, {Flags}    Rd, {Flags}
        //EOR   0001        Rn, OPER2, {Flags}    Rd, {Flags}
        //SUB   0010        Rn, OPER2, {Flags}    Rd, {Flags}
        //RSB   0011        Rn, OPER2, {Flags}    Rd, {Flags}
        //ADD   0100        Rn, OPER2, {Flags}    Rd, {Flags}
        //ORR   1100        Rn, OPER2, {Flags}    Rd, {Flags}
        //BIC   1110        Rn, OPER2, {Flags}    Rd, {Flags}
        AddDPOP(0, DP_R_ROFC, DP_W_RFC);
        AddDPOP(1, DP_R_ROFC, DP_W_RFC);
        AddDPOP(2, DP_R_ROFC, DP_W_RFC);
        AddDPOP(3, DP_R_ROFC, DP_W_RFC);
        AddDPOP(4, DP_R_ROFC, DP_W_RFC);
        AddDPOP(12, DP_R_ROFC, DP_W_RFC);
        AddDPOP(14, DP_R_ROFC, DP_W_RFC);

        //ADC   0101        Rn, OPER2, Flags      Rd, {Flags}
        //SBC   0110        Rn, OPER2, Flags      Rd, {Flags}
        //RSC   0111        Rn, OPER2, Flags      Rd, {Flags}
        AddDPOP(5, DP_R_ROF, DP_W_RFC);
        AddDPOP(6, DP_R_ROF, DP_W_RFC);
        AddDPOP(7, DP_R_ROF, DP_W_RFC);

        //TST   1000 S=1    Rn, OPER2, Flags      Flags
        //TEQ   1001 S=1    Rn, OPER2, Flags      Flags
        AddDPOP(8, DP_R_ROF, DP_W_F);
        AddDPOP(9, DP_R_ROF, DP_W_F);

        //CMP   1010 S=1    Rn, OPER2             Flags
        //CMN   1011 S=1    Rn, OPER2             Flags
        AddDPOP(10, DP_R_ROF, DP_W_F);
        AddDPOP(11, DP_R_ROF, DP_W_F);

        //MOV   1101        OPER2, {Flags}        Rd, {Flags}
        //MVN   1111        OPER2, {Flags}        Rd, {Flags}
        AddDPOP(13, DP_R_OFC, DP_W_RFC);
        AddDPOP(15, DP_R_OFC, DP_W_RFC);
    }

    void* GetMemOp(bool L, bool B)
    {
        if (L)
        {
            if (B)
                return (void*)(u32(DYNACALL*)(Arm7Context*, u32)) ctx->read8;
            else
                return (void*)(u32(DYNACALL*)(Arm7Context*, u32)) ctx->read32;
        }
        else
        {
            if (B)
                return (void*)(u32(DYNACALL*)(Arm7Context*, u32, u32)) ctx->write8;
            else
                return (void*)(u32(DYNACALL*)(Arm7Context*, u32, u32)) ctx->write32;
        }
    }

    //Decodes an opcode, returns type. 
    //opcd might be changed (currently for LDM/STM -> LDR/STR transforms)
    OpType DecodeOpcode(u32& opcd, u32& flags)
    {
        //by default, PC has to be updated
        flags = OP_READS_PC;

        u32 CC = (opcd >> 28);

        if (CC != CC_AL)
            flags |= OP_IS_COND;

        //helpers ...
#define CHK_BTS(M,S,V) ( (M & (opcd>>S)) == (V) ) //Check bits value in opcode
#define IS_LOAD (opcd & (1<<20))                  //Is L bit set ? (LDM/STM LDR/STR)
#define READ_PC_CHECK(S) if (CHK_BTS(15,S,15)) flags|=OP_READS_PC;

//Opcode sets pc ?
        bool _set_pc =
            (CHK_BTS(3, 26, 0) && CHK_BTS(15, 12, 15)) || //Data processing w/ Rd=PC
            (CHK_BTS(3, 26, 1) && CHK_BTS(15, 12, 15) && IS_LOAD) || //LDR/STR w/ Rd=PC 
            (CHK_BTS(7, 25, 4) && (opcd & 32768) && IS_LOAD) || //LDM/STM w/ PC in list	
            CHK_BTS(7, 25, 5) || //B or BL
            CHK_BTS(15, 24, 15);                                    //SWI

        //NV condition means VFP on newer cores, let interpreter handle it...
        if (CC == 15)
            return VOT_Fallback;

        if (_set_pc)
            flags |= OP_SETS_PC;

        //B / BL ?
        if (CHK_BTS(7, 25, 5))
        {
            verify(_set_pc);
            if (!(flags & OP_IS_COND))
                flags &= ~OP_READS_PC;  //not COND doesn't read from pc

            flags |= OP_SETS_PC;        //Branches Set pc ..

            //branch !
            return (opcd & (1 << 24)) ? VOT_BL : VOT_B;
        }

        //Common case: MOVCC PC,REG
        if (CHK_BTS(0xFFFFFF, 4, 0x1A0F00))
        {
            verify(_set_pc);
            if (CC == CC_AL)
                flags &= ~OP_READS_PC;

            return VOT_BR;
        }


        //No support for COND branching opcodes apart from the forms above ..
        if (CC != CC_AL && _set_pc)
        {
            return VOT_Fallback;
        }

        u32 RList = opcd & 0xFFFF;
        u32 Rn = (opcd >> 16) & 15;

#define LDM_REGCNT() (cpuBitsSet[RList & 255] + cpuBitsSet[(RList >> 8) & 255])


        //Data Processing opcodes -- find using mask/key
        //This will eventually be virtualised w/ register renaming
        for (u32 i = 0; i < ops.size(); i++)
        {
            if (!_set_pc && ops[i].key == (opcd & ops[i].mask))
            {
                //We fill in the cases that we have to read pc
                flags &= ~OP_READS_PC;

                //Conditionals always need flags read ...
                if ((opcd >> 28) != 0xE)
                {
                    flags |= OP_HAS_FLAGS_READ;
                    //if (flags & OP_WRITE_REG)
                    flags |= OP_HAS_RD_READ;
                }

                //DPOP !

                if ((ops[i].flags & OP_READ_FLAGS) ||
                    ((ops[i].flags & OP_READ_FLAGS_S) && (opcd & (1 << 20))))
                {
                    flags |= OP_HAS_FLAGS_READ;
                }

                if ((ops[i].flags & OP_WRITE_FLAGS) ||
                    ((ops[i].flags & OP_WRITE_FLAGS_S) && (opcd & (1 << 20))))
                {
                    flags |= OP_HAS_FLAGS_WRITE;
                }

                if (ops[i].flags & OP_WRITE_REG)
                {
                    //All dpops that write, write to RD_12
                    flags |= OP_HAS_RD_12;
                    verify(!(CHK_BTS(15, 12, 15) && CC != CC_AL));
                }

                if (ops[i].flags & OP_READ_REG_1)
                {
                    //Reg 1 is RS_16
                    flags |= OP_HAS_RS_16;

                    //reads from pc ?
                    READ_PC_CHECK(16);
                }

                //op2 is imm or reg ?
                if (!(opcd & (1 << 25)))
                {
                    //its reg (register or imm shifted)
                    flags |= OP_HAS_RS_0;
                    //reads from pc ?
                    READ_PC_CHECK(0);

                    //is it register shifted reg ?
                    if (opcd & (1 << 4))
                    {
                        verify(!(opcd & (1 << 7)));	//must be zero
                        flags |= OP_HAS_RS_8;
                        //can't be pc ...
                        verify(!CHK_BTS(15, 8, 15));
                    }
                    else
                    {
                        //is it RRX ?
                        if (((opcd >> 4) & 7) == 6)
                        {
                            //RRX needs flags to be read (even if the opcode doesn't)
                            flags |= OP_HAS_FLAGS_READ;
                        }
                    }
                }

                return VOT_DataOp;
            }
        }

        //Lets try mem opcodes since its not data processing



        /*
            Lets Check LDR/STR !

            CCCC 01 0 P UBW L Rn Rd Offset	-- LDR/STR (I=0)
        */
        if ((opcd >> 25) == (0xE4 / 2))
        {
            /*
                I=0

                Everything else handled
            */
            arm_printf("ARM: MEM %08X L/S:%d, AWB:%d!\n", opcd, (opcd >> 20) & 1, (opcd >> 21) & 1);

            return VOT_Read;
        }
        else if ((opcd >> 25) == (0xE6 / 2) && CHK_BTS(0x7, 4, 0))
        {
            arm_printf("ARM: MEM REG to Reg %08X\n", opcd);

            /*
                I=1

                Logical Left shift, only
            */
            return VOT_Read;
        }
        //LDM common case
        else if ((opcd >> 25) == (0xE8 / 2) /*&& CHK_BTS(32768,0,0)*/ && CHK_BTS(1, 22, 0) && CHK_BTS(1, 20, 1) && LDM_REGCNT() == 1)
        {
            //P=0
            //U=1
            //L=1
            //W=1
            //S=0

            u32 old_opcd = opcd;

            //One register xfered
            //Can be rewriten as normal mem opcode ..
            opcd = 0xE4000000;

            //Imm offset
            opcd |= 0 << 25;
            //Post incr
            opcd |= old_opcd & (1 << 24);
            //Up/Dn
            opcd |= old_opcd & (1 << 23);
            //Word/Byte
            opcd |= 0 << 22;
            //Write back (must be 0 for PI)
            opcd |= old_opcd & (1 << 21);
            //Load
            opcd |= old_opcd & (1 << 20);

            //Rn
            opcd |= Rn << 16;

            //Rd
            u32 Rd = findfirstset(RList) - 1;
            opcd |= Rd << 12;

            //Offset
            opcd |= 4;

            arm_printf("ARM: MEM TFX R %08X\n", opcd);

            return VOT_Read;
        }
        //STM common case
        else if ((opcd >> 25) == (0xE8 / 2) && CHK_BTS(1, 22, 0) && CHK_BTS(1, 20, 0) && LDM_REGCNT() == 1)
        {
            //P=1
            //U=0
            //L=1
            //W=1
            //S=0

            u32 old_opcd = opcd;

            //One register xfered
            //Can be rewriten as normal mem opcode ..
            opcd = 0xE4000000;

            //Imm offset
            opcd |= 0 << 25;
            //Pre/Post incr
            opcd |= old_opcd & (1 << 24);
            //Up/Dn
            opcd |= old_opcd & (1 << 23);
            //Word/Byte
            opcd |= 0 << 22;
            //Write back
            opcd |= old_opcd & (1 << 21);
            //Store/Load
            opcd |= old_opcd & (1 << 20);

            //Rn
            opcd |= Rn << 16;

            //Rd
            u32 Rd = findfirstset(RList) - 1;
            opcd |= Rd << 12;

            //Offset
            opcd |= 4;

            arm_printf("ARM: MEM TFX W %08X\n", opcd);

            return VOT_Read;
        }
        else if (CHK_BTS(0xE10F0FFF, 0, 0xE10F0000))
        {
            return VOT_MRS;
        }
        else if (CHK_BTS(0xEFBFFFF0, 0, 0xE129F000))
        {
            return VOT_MSR;
        }
        else if ((opcd >> 25) == (0xE8 / 2) && CHK_BTS(32768, 0, 0))
        {
            arm_printf("ARM: MEM FB %08X\n", opcd);
            flags |= OP_MFB; //(flag Just for the fallback counters)
        }
        else
        {
            arm_printf("ARM: FB %08X\n", opcd);
        }

        //by default fallback to interpr
        return VOT_Fallback;
    }

    //very quick-and-dirty register rename based virtualisation
    u32 renamed_regs[16];
    u32 rename_reg_base;

    void RenameRegReset()
    {
        rename_reg_base = r1;
        memset(renamed_regs, 0, sizeof(renamed_regs));
    }

    //returns new reg #. didrn is true if a rename mapping was added
    u32 RenameReg(u32 regn, bool& didrn)
    {
        if (renamed_regs[regn] == 0)
        {
            renamed_regs[regn] = rename_reg_base;
            rename_reg_base++;
            didrn = true;
        }
        else
        {
            didrn = false;
        }

        return renamed_regs[regn];
    }

    //For reg reads (they need to be loaded)
    //load can be used to skip loading (for RD if not cond)
    void LoadAndRename(u32& opcd, u32 bitpos, bool load, u32 pc)
    {
        bool didrn;
        u32 reg = (opcd >> bitpos) & 15;

        u32 nreg = RenameReg(reg, didrn);

        opcd = (opcd & ~(15 << bitpos)) | (nreg << bitpos);

        if (load && didrn)
        {
            if (reg == 15)
                armv->MOV32((eReg)nreg, pc);
            else
                armv->LoadReg((eReg)nreg, reg);
        }
    }

    //For results store (they need to be stored)
    void StoreAndRename(u32 opcd, u32 bitpos)
    {
        bool didrn;
        u32 reg = (opcd >> bitpos) & 15;

        u32 nreg = RenameReg(reg, didrn);

        verify(!didrn);

        if (reg == 15)
            reg = R15_ARM_NEXT;

        armv->StoreReg((eReg)nreg, reg);
    }

    //Virtualise Data Processing opcode
    void VirtualizeOpcode(u32 opcd, u32 flag, u32 pc)
    {
        //Keep original opcode for info
        u32 orig = opcd;

        //Load arm flags, RS0/8/16, RD12/16 (as indicated by the decoder flags)

        if (flag & OP_HAS_FLAGS_READ)
        {
            armv->LoadFlags();
        }

        if (flag & OP_HAS_RS_0)
            LoadAndRename(opcd, 0, true, pc + 8);
        if (flag & OP_HAS_RS_8)
            LoadAndRename(opcd, 8, true, pc + 8);
        if (flag & OP_HAS_RS_16)
            LoadAndRename(opcd, 16, true, pc + 8);

        if (flag & OP_HAS_RD_12)
            LoadAndRename(opcd, 12, flag & OP_HAS_RD_READ, pc + 4);

        if (flag & OP_HAS_RD_16)
        {
            verify(!(flag & OP_HAS_RS_16));
            LoadAndRename(opcd, 16, flag & OP_HAS_RD_READ, pc + 4);
        }

        //Opcode has been modified to use the new regs
        //Emit it ...
        arm_printf("Arm Virtual: %08X -> %08X\n", orig, opcd);
        armEmit32(opcd);

        //Store arm flags, rd12/rd16 (as indicated by the decoder flags)
        if (flag & OP_HAS_RD_12)
            StoreAndRename(orig, 12);

        if (flag & OP_HAS_RD_16)
            StoreAndRename(orig, 16);

        //Sanity check ..
        if (renamed_regs[15] != 0)
        {
            verify(flag & OP_READS_PC || (flag & OP_SETS_PC && !(flag & OP_IS_COND)));
        }

        if (flag & OP_HAS_FLAGS_WRITE)
            armv->StoreFlags();
    }

    void DumpRegs(const char* output)
    {
        static FILE* f = fopen(output, "w");
        static int id = 0;
#if 0
        if (490710 == id)
        {
            __asm int 3;
        }
#endif
        verify(id != 137250);
#if 1
        fprintf(f, "%d\n", id);
        //for(int i=0;i<14;i++)
        {
            int i = R15_ARM_NEXT;
            fprintf(f, "r%d=%08X\n", i, arm_reg[i].I);
        }
#endif
        id++;
    }

    void DYNACALL PrintOp(u32 opcd)
    {
        printf("%08X\n", opcd);
    }

   
#undef r

    /*
        TODO:
        R15 read/writing is kind of .. weird
        Gotta investigate why ..
    */

    //Mem operand 2 calculation, if Reg or large imm
    void MemOperand2(eReg dst, bool I, bool U, u32 offs, u32 opcd)
    {
        if (I == true)
        {
            u32 Rm = (opcd >> 0) & 15;
            verify(CHK_BTS(7, 4, 0));// only SHL mode
            armv->LoadReg(r1, Rm);
            u32 SA = 31 & (opcd >> 7);
            //can't do shifted add for now -- EMITTER LIMIT --
            if (SA)
                armv->lsl(r1, r1, SA);
        }
        else
        {
            armv->MOV32(r1, offs);
        }

        if (U)
            armv->add(dst, r0, r1);
        else
            armv->sub(dst, r0, r1);
    }

  

    //Compile & run block of code, starting armNextPC
    void CompileCode()
    {
        //emitter/block setup
        armv->setup();

        //Get the code ptr
        void* rv = EMIT_GET_PTR();

        //update the block table
        EntryPoints[(armNextPC & ctx->aram_mask) / 4] = rv;

        //setup local pc counter
        u32 pc = armNextPC;

        //the ops counter is used to terminate the block (max op count for a single block is 32 currently)
        //We don't want too long blocks for timing accuracy
        u32 ops = 0;

        u32 Cycles = 0;

        for (;;)
        {
            ops++;

            //Read opcode ...
            u32 opcd = CPUReadMemoryQuick(pc);

#if HOST_CPU==CPU_X86
            //Sanity check: Stale cache
            armv->check_cache(opcd, pc);
#endif

            u32 op_flags;

            //Decode & handle opcode

            OpType opt = DecodeOpcode(opcd, op_flags);

            switch (opt)
            {
            case VOT_DataOp:
            {
                //data processing opcode that can be virtualised
                RenameRegReset();

                /*
                if (op_flags & OP_READS_PC)
                    armv->imm_to_reg(15,pc+8);

                else*/
#if HOST_CPU==CPU_X86
                armv->imm_to_reg(15, rand());
#endif

                VirtualizeOpcode(opcd, op_flags, pc);

#if HOST_CPU==CPU_X86
                armv->imm_to_reg(15, rand());
#endif
            }
            break;

            case VOT_BR:
            {
                //Branch to reg
                ConditionCode cc = (ConditionCode)(opcd >> 28);

                verify(op_flags & OP_SETS_PC);

                if (cc != CC_AL)
                {
                    armv->LoadFlags();
                    armv->imm_to_reg(R15_ARM_NEXT, pc + 4);
                }

                armv->LoadReg(r0, opcd & 0xF);

                armv->bic(r0, r0, 3);

                void* ref = armv->start_conditional(cc);
                armv->StoreReg(r0, R15_ARM_NEXT, cc);
                armv->end_conditional(ref);
            }
            break;

            case VOT_B:
            case VOT_BL:
            {
                //Branch to imm

                //<<2, sign extend !
                s32 offs = ((s32)opcd << 8) >> 6;

                if (op_flags & OP_IS_COND)
                {
                    armv->imm_to_reg(R15_ARM_NEXT, pc + 4);
                    armv->LoadFlags();
                    ConditionCode cc = (ConditionCode)(opcd >> 28);
                    void* ref = armv->start_conditional(cc);
                    if (opt == VOT_BL)
                    {
                        armv->MOV32(r0, pc + 4);
                        armv->StoreReg(r0, 14, cc);
                    }

                    armv->MOV32(r0, pc + 8 + offs);
                    armv->StoreReg(r0, R15_ARM_NEXT, cc);
                    armv->end_conditional(ref);
                }
                else
                {
                    if (opt == VOT_BL)
                        armv->imm_to_reg(14, pc + 4);

                    armv->imm_to_reg(R15_ARM_NEXT, pc + 8 + offs);
                }
            }
            break;

            case VOT_Read:
            {
                //LDR/STR

                u32 offs = opcd & 4095;
                bool U = opcd & (1 << 23);
                bool Pre = opcd & (1 << 24);

                bool W = opcd & (1 << 21);
                bool I = opcd & (1 << 25);

                u32 Rn = (opcd >> 16) & 15;
                u32 Rd = (opcd >> 12) & 15;

                bool DoWB = W || (!Pre && Rn != Rd);	//Write back if: W, Post update w/ Rn!=Rd
                bool DoAdd = DoWB || Pre;

                //Register not updated anyway
                if (I == false && offs == 0)
                {
                    DoWB = false;
                    DoAdd = false;
                }

                //verify(Rd!=15);
                verify(!((Rn == 15) && DoWB));

                //AGU
                if (Rn != 15)
                {
                    armv->LoadReg(r1, Rn);

                    if (DoAdd)
                    {
                        eReg dst = Pre ? r1 : r9;

                        if (I == false && is_i8r4(offs))
                        {
                            if (U)
                                armv->add(dst, r1, offs);
                            else
                                armv->add(dst, r1, -offs);
                        }
                        else
                        {
                            MemOperand2(dst, I, U, offs, opcd);
                        }

                        if (DoWB && dst == r1)
                            armv->mov(r9, r1);
                    }
                }
                else
                {
                    u32 addr = pc + 8;

                    if (Pre && offs && I == false)
                    {
                        addr += U ? offs : -offs;
                    }

                    armv->MOV32(r1, addr);

                    if (Pre && I == true)
                    {
                        MemOperand2(r2, I, U, offs, opcd);
                        armv->add(r1, r1, r2);
                    }
                }

                if (CHK_BTS(1, 20, 0))
                {
                    if (Rd == 15)
                    {
                        armv->MOV32(r2, pc + 12);
                    }
                    else
                    {
                        armv->LoadReg(r2, Rd);
                    }
                }
                //Call handler

                //FIXME param 0
                armv->MOV32(r0, (uintptr_t)ctx);
                if (CHK_BTS(1, 20, 1))
                    armv->call(GetMemOp(CHK_BTS(1, 20, 1), CHK_BTS(1, 22, 1)), 2, 1);
                else
                    armv->call(GetMemOp(CHK_BTS(1, 20, 1), CHK_BTS(1, 22, 1)), 3, 0);

                if (CHK_BTS(1, 20, 1))
                {
                    if (Rd == 15)
                    {
                        verify(op_flags & OP_SETS_PC);
                        armv->StoreReg(r0, R15_ARM_NEXT);
                    }
                    else
                    {
                        armv->StoreReg(r0, Rd);
                    }
                }

                //Write back from AGU, if any
                if (DoWB)
                {
                    armv->StoreReg(r9, Rn);
                }
            }
            break;

            case VOT_MRS:
            {
                u32 Rd = (opcd >> 12) & 15;

                armv->MOV32(r0, (uintptr_t)ctx);
                armv->call((void*)ARM7Backend::CPUUpdateCPSR, 1, 0);

                if (opcd & (1 << 22))
                {
                    armv->LoadReg(r0, 17);
                }
                else
                {
                    armv->LoadReg(r0, 16);
                }

                armv->StoreReg(r0, Rd);
            }
            break;

            case VOT_MSR:
            {
                u32 Rm = (opcd >> 0) & 15;

                //FIXME PARAM 0
                armv->MOV32(r0, (uintptr_t)ctx);
                armv->LoadReg(r1, Rm);
                if (opcd & (1 << 22))
                    armv->call((void*)(void (DYNACALL*)(u32)) & MSR_do<1>, 2, 0);
                else
                    armv->call((void*)(void (DYNACALL*)(u32)) & MSR_do<0>, 2, 0);

                if (op_flags & OP_SETS_PC)
                    armv->imm_to_reg(R15_ARM_NEXT, pc + 4);
            }
            break;

            case VOT_Fallback:
            {
                //interpreter fallback

                //arm_single_op needs PC+4 on r15
                //TODO: only write it if needed -> Probably not worth the code, very few fallbacks now...
                armv->imm_to_reg(15, pc + 8);

                //For cond branch, MSR
                if (op_flags & OP_SETS_PC)
                    armv->imm_to_reg(R15_ARM_NEXT, pc + 4);

#if HOST_CPU==CPU_X86
                if (!(op_flags & OP_SETS_PC))
                    armv->imm_to_reg(R15_ARM_NEXT, pc + 4);
#endif

                armv->intpr(opcd);

#if HOST_CPU==CPU_X86
                if (!(op_flags & OP_SETS_PC))
                {
                    //Sanity check: next pc
                    armv->check_pc(pc + 4);
#if 0
                    x86e->Emit(op_mov32, ECX, opcd);
                    x86e->Emit(op_call, x86_ptr_imm(PrintOp));
#endif
                }
#endif
            }
            break;

            default:
                die("can't happen\n");
            }

            //Lets say each opcode takes 9 cycles for now ..
            Cycles += 9;

#if HOST_CPU==CPU_X86
            armv->imm_to_reg(15, 0xF87641FF);

            armv->prof(opt, opcd, op_flags);
#endif

            //Branch ?
            if (op_flags & OP_SETS_PC)
            {
                //x86e->Emit(op_call,x86_ptr_imm(DumpRegs)); // great debugging tool
                arm_printf("ARM: %06X: Block End %d\n", pc, ops);

#if HOST_CPU==CPU_X86 && 0
                //Great fallback finder, also spams console
                if (opt == VOT_Fallback)
                {
                    x86e->Emit(op_mov32, ECX, opcd);
                    x86e->Emit(op_call, x86_ptr_imm(PrintOp));
                }
#endif
                break;
            }

            //block size limit ?
            if (ops > 32)
            {
                arm_printf("ARM: %06X: Block split %d\n", pc, ops);

                armv->imm_to_reg(R15_ARM_NEXT, pc + 4);
                break;
            }

            //Goto next opcode
            pc += 4;
        }

        armv->end(&lps, (void*)rv, Cycles);
    }



    void InvalidateJitCache()
    {
        armv->InvalidateJitCache();

        printf("ARM7: Invalidating cache\n");
        for (u32 i = 0; i < ARRAY_SIZE(EntryPoints); i++)
            EntryPoints[i] = lps.compilecode;
    }

    void armt_init()
    {
        InitHash();
    }
};

ARM7Backend* ARM7Backend::CreateJit(Arm7Context* ctx) {
    return new Arm7JitVirt_impl(ctx);
}

void DYNACALL CompileCode(Arm7JitVirt_impl* arm) {
    arm->CompileCode();
}

#endif