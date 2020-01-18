#pragma once

#include "jit/emitter/arm32/arm_coding.h"

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


struct Looppoints
{
    void* compilecode;
    void* mainloop;
    void* dispatch;
    void* exit;
};

struct ARM7Backend;
struct Arm7Context;

struct Arm7VirtBackend {

    static Arm7VirtBackend* Create(ARM7Backend* arm, Arm7Context* ctx);

    virtual void GenerateLooppoints(Looppoints* lp) = 0;

    virtual void* armGetEmitPtr() = 0;

    virtual void LoadReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL) = 0;
    virtual void StoreReg(ARM::eReg rd, u32 regn, ARM::ConditionCode cc = ARM::CC_AL) = 0;

    virtual void LoadFlags() = 0;

    virtual void StoreFlags() = 0;

    virtual void mov(ARM::eReg regd, ARM::eReg regn) = 0;

    virtual void sxtb(ARM::eReg regd, ARM::eReg regs) = 0;

    virtual void zxtb(ARM::eReg regd, ARM::eReg regs) = 0;

    virtual void add(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm) = 0;

    virtual void sub(ARM::eReg regd, ARM::eReg regn, ARM::eReg regm) = 0;

    virtual void add(ARM::eReg regd, ARM::eReg regn, s32 imm) = 0;

    virtual void lsl(ARM::eReg regd, ARM::eReg regn, u32 imm) = 0;

    virtual void bic(ARM::eReg regd, ARM::eReg regn, u32 imm) = 0;

    virtual void* start_conditional(ARM::ConditionCode cc) = 0;
    virtual void end_conditional(void* ref) = 0;

    virtual void imm_to_reg(u32 regn, u32 imm) = 0;

    virtual void MOV32(ARM::eReg regn, u32 imm) = 0;

    virtual void call(void* loc, int params, int returns) = 0;

    virtual void setup() = 0;

    virtual void intpr(u32 opcd) = 0;

    virtual void end(Looppoints* lp, void* codestart, u32 cycles) = 0;

    //sanity check: non branch doesn't set pc
    virtual void check_pc(u32 pc) = 0;

    //sanity check: stale cache
    virtual void check_cache(u32 opcd, u32 pc) = 0;

    //profiler hook
    virtual void prof(OpType opt, u32 op, u32 flags) = 0;

    virtual void Emit32(u32 emit32) = 0;

    virtual void InvalidateJitCache() = 0;
};

extern Arm7VirtBackend* virtBackend;

struct Arm7JitVirt_impl;
void DYNACALL CompileCode(Arm7JitVirt_impl* arm);