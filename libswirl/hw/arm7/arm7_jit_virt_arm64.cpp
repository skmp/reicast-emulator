/*
	This file is part of libswirl
        original code by flyinghead
*/
#include "license/bsd"



#include "build.h"
#if HOST_CPU == CPU_ARM64 && FEAT_AREC != DYNAREC_NONE

#include <sstream>
#include "arm7.h"
#include "arm7_context.h"
#include "arm7_jit_virt_backend.h"
#include "jit/emitter/arm32/arm_coding.h"
#include "deps/vixl/aarch64/macro-assembler-aarch64.h"

#if HOST_OS == OS_LINUX || HOST_OS == OS_DARWIN
#include <sys/mman.h>
#endif
#if HOST_OS == OS_WINDOWS
#include <Windows.h>
#endif


using namespace vixl::aarch64;
//#include "deps/vixl/aarch32/disasm-aarch32.h"

extern void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);


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



extern "C" void armFlushICache(void *bgn, void *end) {
	vmem_platform_flush_cache(bgn, end, bgn, end);
}

struct Arm7JitArm7VirtBackendArm64 : Arm7VirtBackend {

    MacroAssembler* assembler;
    ARM7Backend* arm;
    Arm7Context* ctx;

    u8* ICache;
    u8* icPtr_Base;
    u8* icPtr;

   
    Arm7JitArm7VirtBackendArm64(ARM7Backend* arm, Arm7Context* ctx) : arm(arm), ctx(ctx) {

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
            perror("\n\tError - Couldn't mprotect ARM7_TCB!");
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
        return (ARM::eReg)25;
    }

    void GenerateLooppoints(Looppoints* lp) {
        void* codestart = icPtr;
        assembler = new MacroAssembler(icPtr, ICache + ICacheSize - icPtr);

        // generate the loop points here

        lp->dispatch = assembler->GetCursorAddress<void*>();
        // arm_dispatch
        {
            
            //"arm_dispatch:							\n\t"
            //"ldp w0, w1, [x28, #184]			\n\t"	// load Next PC, interrupt
            assembler->Ldp(w0, w1, MemOperand(x28, 184));
            if ((ctx->aram_mask + 1) == 2 * 1024 * 1024) {
                //"ubfx w2, w0, #2, #19				\n\t"	// w2 = pc >> 2. Note: assuming address space == 2 MB (21 bits)
                assembler->Ubfx(w2, w0, 2, 19);
            }
            else if ((ctx->aram_mask + 1) == 8 * 1024 * 1024) {
                //"ubfx w2, w0, #2, #21				\n\t"	// w2 = pc >> 2. Note: assuming address space == 8 MB (23 bits)
                assembler->Ubfx(w2, w0, 2, 21);
            }
            else {
                die("Unsupported AICA RAM size");
            }

            Label arm_dofiq;
            //"cbnz w1, arm_dofiq					\n\t"	// if interrupt pending, handle it
            assembler->Cbnz(w1, &arm_dofiq);
            //"add x2, x26, x2, lsl #3			\n\t"	// x2 = EntryPoints + pc << 1
            assembler->Add(x2, x26, Operand(x2, LSL, 3));
            //"ldr x3, [x2]						\n\t"
            assembler->Ldr(x3, MemOperand(x2));
            //"br x3								\n"
            assembler->Br(x3);

            assembler->Bind(&arm_dofiq);
            //"arm_dofiq:								\n\t"
            // mov x0, ctx
            assembler->Mov(x0, (uintptr_t)ctx);
            //"bl CPUFiq							\n\t"
            ptrdiff_t offset = reinterpret_cast<uintptr_t>(&ARM7Backend::CPUFiq) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label CPUFiq_label;
            assembler->BindToOffset(&CPUFiq_label, offset);
            assembler->Bl(&CPUFiq_label);
            //"b arm_dispatch						\n\t"
            offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label arm_dispatch_label;
            assembler->BindToOffset(&arm_dispatch_label, offset);
            assembler->B(&arm_dispatch_label);
        }

        lp->mainloop = assembler->GetCursorAddress<void*>();
        // arm_mainloop
        {
            //"stp x25, x26, [sp, #-48]!			\n\t"
            assembler->Stp(x25, x26, MemOperand(sp, -48, PreIndex));
            //"stp x27, x28, [sp, #16]			\n\t"
            assembler->Stp(x27, x28, MemOperand(sp, 16));
            //"stp x29, x30, [sp, #32]			\n\t"
            assembler->Stp(x29, x30, MemOperand(sp, 32));

            //"mov x28, x1						\n\t"	// arm7 registers
            assembler->Mov(x28, (uintptr_t)ctx);
            //"mov x26, x2						\n\t"	// lookup base
            assembler->Mov(x26, (uintptr_t)arm->GetEntrypointBase());

            //"ldr w27, [x28, #192]				\n\t"	// cycle count
            assembler->Ldr(w27, MemOperand(x28, 192));
            //"add w27, w27, w0					\n"		// add cycles for this timeslice
            assembler->Add(w27, w27, w0);

            //"b arm_dispatch				\n\t"
            ptrdiff_t offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
            Label arm_dispatch_label;
            assembler->BindToOffset(&arm_dispatch_label, offset);
            assembler->B(&arm_dispatch_label);
        }

        lp->compilecode = assembler->GetCursorAddress<void*>();
        // arm_compilecode
        {
            assembler->Mov(x0, (uintptr_t)arm);
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
            //assembler->Brk(0);
            //"str w27, [x28, #192]				\n\t"	// if timeslice is over, save remaining cycles
            assembler->Str(w27, MemOperand(x28, 192));
            //"ldp x29, x30, [sp, #32]			\n\t"
            assembler->Ldp(x29, x30, MemOperand(sp, 32));
            //"ldp x27, x28, [sp, #16]			\n\t"
            assembler->Ldp(x27, x28, MemOperand(sp, 16));
            //"ldp x25, x26, [sp], #48			\n\t"
            assembler->Ldp(x25, x26, MemOperand(sp, 48, PostIndex));
            //"ret								\n"
            assembler->Ret();
        }


        // cleanup

        assembler->FinalizeCode();
        verify(assembler->GetBuffer()->GetCursorOffset() <= assembler->GetBuffer()->GetCapacity());
        vmem_platform_flush_cache(
            codestart, assembler->GetCursorAddress<void*>(),
            codestart, assembler->GetCursorAddress<void*>());
        
        icPtr += assembler->GetBuffer()->GetSizeInBytes();
        icPtr_Base = icPtr;

        delete assembler;
        assembler = nullptr;
    }

    MemOperand arm_reg_operand(u32 regn)
    {
        return MemOperand(x28, (u8*)&ctx->regs[regn].I - (u8*)&ctx->regs[0].I);
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
        assembler->Ldr(Register::GetWRegFromCode(rd), arm_reg_operand(regn));
    }
    void StoreReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL)
    {
        assembler->Str(Register::GetWRegFromCode(rd), arm_reg_operand(regn));
    }

    void* start_conditional(ARM::ConditionCode cc)
    {
        if (cc == ARM::CC_AL)
            return NULL;
        Label* label = new Label();
        verify(cc <= ARM::CC_LE);
        Condition condition = (Condition)((u32)cc ^ 1);
        assembler->B(label, condition);

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

    //For COND
    void LoadFlags()
    {
        //Load flags
        LoadReg(ARM::r0, RN_PSR_FLAGS);
        //move them to flags register
        assembler->Msr(NZCV, x0);
    }

    void StoreFlags()
    {
        //get results from flags register
        assembler->Mrs(x1, NZCV);
        //Store flags
        StoreReg(ARM::r1, RN_PSR_FLAGS);
    }

    void imm_to_reg(u32 regn, u32 imm)
    {
        assembler->Mov(w0, imm);
        assembler->Str(w0, arm_reg_operand(regn));
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
        assembler->Mov(x0, (uintptr_t)ctx);
        assembler->Mov(w1, opcd);
        call((void*)&ARM7Backend::singleOp, 1, 1);
    }

    void end(Looppoints* lp, void* codestart, u32 cycl)
    {
        //Normal block end
        //cycle counter rv

        //pop registers & return
        assembler->Subs(w27, w27, cycl);
        ptrdiff_t offset = reinterpret_cast<uintptr_t>(lp->exit) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
        Label arm_exit_label;
        assembler->BindToOffset(&arm_exit_label, offset);
        assembler->B(&arm_exit_label, mi);	//statically predicted as not taken

        offset = reinterpret_cast<uintptr_t>(lp->dispatch) - assembler->GetBuffer()->GetStartAddress<uintptr_t>();
        Label arm_dispatch_label;
        assembler->BindToOffset(&arm_dispatch_label, offset);
        assembler->B(&arm_dispatch_label);

        assembler->FinalizeCode();
        verify(assembler->GetBuffer()->GetCursorOffset() <= assembler->GetBuffer()->GetCapacity());
        vmem_platform_flush_cache(
            codestart, assembler->GetCursorAddress<void*>(),
            codestart, assembler->GetCursorAddress<void*>());
        icPtr += assembler->GetBuffer()->GetSizeInBytes();

#if 0
        Instruction* instr_start = (Instruction*)codestart;
        Instruction* instr_end = assembler->GetCursorAddress<void*>();
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
        assembler->Mov(Register::GetXRegFromCode(regn), imm);
    }

    void MOV32(ARM::eReg regn, u32 imm)
    {
        assembler->Mov(Register::GetWRegFromCode(regn), imm);
    }


    void mov(ARM::eReg regd, ARM::eReg regn)
    {
        assembler->Mov(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn));
    }

    virtual void sxtb(ARM::eReg regd, ARM::eReg regs)
    {
        assembler->Sxtb(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regs));
    }

    virtual void zxtb(ARM::eReg regd, ARM::eReg regs)
    {
        assembler->And(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regs), 0xFF);
    }


    void add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        assembler->Add(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn), Register::GetWRegFromCode(regm));
    }

    void sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm)
    {
        assembler->Sub(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn), Register::GetWRegFromCode(regm));
    }

    void add(ARM::eReg regd, ARM::eReg regn, s32 imm)
    {
        assembler->Add(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn), imm);
    }

    void lsl(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        assembler->Lsl(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn), imm);
    }

    void bic(ARM::eReg regd, ARM::eReg regn, u32 imm)
    {
        assembler->Bic(Register::GetWRegFromCode(regd), Register::GetWRegFromCode(regn), imm);
    }

    class android_buf : public std::stringbuf
    {
    public:
        virtual int sync() override {
            printf("ARM7: %s\n", this->str().c_str());
            str("");

            return 0;
        }
    };

    void Emit32(u32 opcode)
    {
#if 0
        if (opcode != 0x00011001)
        {
            android_buf buffer;
            std::ostream cout(&buffer);
            vixl::aarch32::PrintDisassembler disasm(cout, 0);
            disasm.DecodeA32(opcode);
            cout.flush();
        }
#endif

        const Register& rd = Register::GetWRegFromCode((opcode >> 12) & 15);
        const Register& rn = Register::GetWRegFromCode((opcode >> 16) & 15);
        bool set_flags = opcode & (1 << 20);
        Operand op2;
        int op_type = (opcode >> 21) & 15;
        bool logical_op = op_type == 0 || op_type == 1 || op_type == 8 || op_type == 9	// AND, EOR, TST, TEQ
            || op_type == 12 || op_type == 13 || op_type == 15 || op_type == 14;	// ORR, MOV, MVN, BIC
        bool set_carry_bit = false;

        ARM::ConditionCode condition = (ARM::ConditionCode)(opcode >> 28);
        void* cond_op_label = start_conditional(condition);

        if (opcode & (1 << 25))
        {
            // op2 is imm8r4
            u32 rotate = ((opcode >> 8) & 15) << 1;
            u32 imm8 = opcode & 0xff;
            op2 = Operand((imm8 >> rotate) | (imm8 << (32 - rotate)));
        }
        else
        {
            // op2 is register
            const Register& rm = Register::GetWRegFromCode(opcode & 15);

            Shift shift = (Shift)((opcode >> 5) & 3);

            if (opcode & (1 << 4))
            {
                // shift by register
                // FIXME Carry must be set based on shift/rotate
                //if (set_flags && logical_op)
                //	die("shift by register with set flags C - not implemented");
                const Register& shift_reg = Register::GetWRegFromCode((opcode >> 8) & 15);

                Label shift_by_32_label;

                switch (shift)
                {
                case LSL:
                case LSR:
                    assembler->Mrs(x0, NZCV);
                    assembler->Cmp(shift_reg, 32);
                    if (shift == LSL)
                        assembler->Lsl(w15, rm, shift_reg);
                    else
                        assembler->Lsr(w15, rm, shift_reg);
                    assembler->Csel(w15, 0, w15, ge);		// LSL and LSR by 32 or more gives 0
                    assembler->Msr(NZCV, x0);
                    break;
                case ASR:
                    assembler->Mrs(x0, NZCV);
                    assembler->Cmp(shift_reg, 32);
                    assembler->Asr(w15, rm, shift_reg);
                    assembler->Sbfx(w13, rm, 31, 1);
                    assembler->Csel(w15, w13, w15, ge);		// ASR by 32 or more gives 0 or -1 depending on operand sign
                    assembler->Msr(NZCV, x0);
                    break;
                case ROR:
                    assembler->Ror(w15, rm, shift_reg);
                    break;
                default:
                    die("Invalid shift");
                    break;
                }
                op2 = Operand(w15);
            }
            else
            {
                // shift by immediate
                u32 shift_imm = (opcode >> 7) & 0x1f;
                if (shift != ROR && shift_imm != 0 && !(set_flags && logical_op))
                {
                    op2 = Operand(rm, shift, shift_imm);
                }
                else if (shift_imm == 0)
                {
                    if (shift == LSL)
                    {
                        op2 = Operand(rm);		// LSL 0 is a no-op
                    }
                    else
                    {
                        // Shift by 32
                        if (set_flags && logical_op)
                            set_carry_bit = true;
                        if (shift == LSR)
                        {
                            if (set_flags && logical_op)
                                assembler->Ubfx(w14, rm, 31, 1);			// w14 = rm[31]
                            assembler->Mov(w15, 0);							// w15 = 0
                        }
                        else if (shift == ASR)
                        {
                            if (set_flags && logical_op)
                                assembler->Ubfx(w14, rm, 31, 1);			// w14 = rm[31]
                            assembler->Sbfx(w15, rm, 31, 1);				// w15 = rm < 0 ? -1 : 0
                        }
                        else if (shift == ROR)
                        {
                            // RRX
                            assembler->Cset(w14, cs);						// w14 = C
                            assembler->Mov(w15, Operand(rm, LSR, 1));		// w15 = rm >> 1
                            assembler->Bfi(w15, w14, 31, 1);				// w15[31] = C
                            if (set_flags && logical_op)
                                assembler->Ubfx(w14, rm, 0, 1);				// w14 = rm[0] (new C)
                        }
                        else
                            die("Invalid shift");
                        op2 = Operand(w15);
                    }
                }
                else
                {
                    // Carry must be preserved or Ror shift
                    if (set_flags && logical_op)
                        set_carry_bit = true;
                    if (shift == LSL)
                    {
                        assembler->Ubfx(w14, rm, 32 - shift_imm, 1);	// w14 = rm[lsb]
                        assembler->Lsl(w15, rm, shift_imm);				// w15 <<= shift
                    }
                    else
                    {
                        if (set_flags && logical_op)
                            assembler->Ubfx(w14, rm, shift_imm - 1, 1);	// w14 = rm[msb]

                        if (shift == LSR)
                            assembler->Lsr(w15, rm, shift_imm);			// w15 >>= shift
                        else if (shift == ASR)
                            assembler->Asr(w15, rm, shift_imm);
                        else if (shift == ROR)
                            assembler->Ror(w15, rm, shift_imm);
                        else
                            die("Invalid shift");
                    }
                    op2 = Operand(w15);
                }
            }
        }
        if (!set_carry_bit
            && (op_type == 8 || op_type == 9			// TST and TEQ always set flags
                || (logical_op && set_flags)))
        {
            // Logical ops should only affect the carry bit based on the op2 shift
            // Here we're not shifting so the carry bit should be preserved
            set_carry_bit = true;
            assembler->Cset(w14, cs);
        }

        switch (op_type)
        {
        case 0:		// AND
            if (set_flags)
                assembler->Ands(rd, rn, op2);
            else
                assembler->And(rd, rn, op2);
            break;
        case 1:		// EOR
            assembler->Eor(rd, rn, op2);
            if (set_flags)
                assembler->Tst(rd, rd);
            break;
        case 2:		// SUB
            if (set_flags)
                assembler->Subs(rd, rn, op2);
            else
                assembler->Sub(rd, rn, op2);
            break;
        case 3:		// RSB
            assembler->Neg(w0, rn);
            if (set_flags)
                assembler->Adds(rd, w0, op2);
            else
                assembler->Add(rd, w0, op2);
            break;
        case 4:		// ADD
            if (set_flags)
                assembler->Adds(rd, rn, op2);
            else
                assembler->Add(rd, rn, op2);
            break;
        case 12:	// ORR
            assembler->Orr(rd, rn, op2);
            if (set_flags)
                assembler->Tst(rd, rd);
            break;
        case 14:	// BIC
            if (set_flags)
                assembler->Bics(rd, rn, op2);
            else
                assembler->Bic(rd, rn, op2);
            break;
        case 5:		// ADC
            if (set_flags)
                assembler->Adcs(rd, rn, op2);
            else
                assembler->Adc(rd, rn, op2);
            break;
        case 6:		// SBC
            if (set_flags)
                assembler->Sbcs(rd, rn, op2);
            else
                assembler->Sbc(rd, rn, op2);
            break;
        case 7:		// RSC
            assembler->Ngc(w0, rn);
            if (set_flags)
                assembler->Adds(rd, w0, op2);
            else
                assembler->Add(rd, w0, op2);
            break;
        case 8:		// TST
            assembler->Tst(rn, op2);
            break;
        case 9:		// TEQ
            assembler->Eor(w0, rn, op2);
            assembler->Tst(w0, w0);
            break;
        case 10:	// CMP
            assembler->Cmp(rn, op2);
            break;
        case 11:	// CMN
            assembler->Cmn(rn, op2);
            break;
        case 13:	// MOV
            assembler->Mov(rd, op2);
            if (set_flags)
                assembler->Tst(rd, rd);
            break;
        case 15:	// MVN
            assembler->Mvn(rd, op2);
            if (set_flags)
                assembler->Tst(rd, rd);
            break;
        }
        if (set_carry_bit)
        {
            assembler->Mrs(x0, NZCV);
            assembler->Bfi(x0, x14, 29, 1);		// C is bit 29 in NZCV
            assembler->Msr(NZCV, x0);
        }
        end_conditional(cond_op_label);
    }


    //sanity check: non branch doesn't set pc
    virtual void check_pc(u32 pc) { }

    //sanity check: stale cache
    virtual void check_cache(u32 opcd, u32 pc) { }

    //profiler hook
    virtual void prof(OpType opt, u32 op, u32 flags) { }

    virtual void InvalidateJitCache() {
        icPtr = icPtr_Base;
    }
};

Arm7VirtBackend* Arm7VirtBackend::Create(ARM7Backend* arm, Arm7Context* ctx) {
    return new Arm7JitArm7VirtBackendArm64(arm, ctx);
}
#if FIXME_ARM7JIT
//
// Dynarec main loop
//
// w25 is used for temp mem save (post increment op2)
// x26 is the entry points table
// w27 is the cycle counter
// x28 points to the arm7 registers base
__asm__(
    ".globl arm_compilecode				\n\t"
    ".hidden arm_compilecode			\n"
    "arm_compilecode:						\n\t"
    "bl CompileCode						\n\t"
    "b arm_dispatch						\n\t"

    ".globl arm_mainloop				\n\t"
    ".hidden arm_mainloop				\n"
    "arm_mainloop:							\n\t"	//  arm_mainloop(cycles, regs, entry points)
    "stp x25, x26, [sp, #-48]!			\n\t"
    "stp x27, x28, [sp, #16]			\n\t"
    "stp x29, x30, [sp, #32]			\n\t"

    "mov x28, x1						\n\t"	// arm7 registers
    "mov x26, x2						\n\t"	// lookup base

    "ldr w27, [x28, #192]				\n\t"	// cycle count
    "add w27, w27, w0					\n"		// add cycles for this timeslice

    ".globl arm_dispatch				\n\t"
    ".hidden arm_dispatch				\n"
    "arm_dispatch:							\n\t"
    "ldp w0, w1, [x28, #184]			\n\t"	// load Next PC, interrupt
#if ARAM_SIZE == 2*1024*1024
    "ubfx w2, w0, #2, #19				\n\t"	// w2 = pc >> 2. Note: assuming address space == 2 MB (21 bits)
#elif ARAM_SIZE == 8*1024*1024
    "ubfx w2, w0, #2, #21				\n\t"	// w2 = pc >> 2. Note: assuming address space == 8 MB (23 bits)
#else
#error Unsupported AICA RAM size
#endif
    "cbnz w1, arm_dofiq					\n\t"	// if interrupt pending, handle it

    "add x2, x26, x2, lsl #3			\n\t"	// x2 = EntryPoints + pc << 1
    "ldr x3, [x2]						\n\t"
    "br x3								\n"

    "arm_dofiq:								\n\t"
    "bl CPUFiq							\n\t"
    "b arm_dispatch						\n\t"

    ".globl arm_exit					\n\t"
    ".hidden arm_exit					\n"
    "arm_exit:								\n\t"
    "str w27, [x28, #192]				\n\t"	// if timeslice is over, save remaining cycles
    "ldp x29, x30, [sp, #32]			\n\t"
    "ldp x27, x28, [sp, #16]			\n\t"
    "ldp x25, x26, [sp], #48			\n\t"
    "ret								\n"
);
#endif // ARM64
#endif
