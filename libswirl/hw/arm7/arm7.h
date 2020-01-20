#pragma once
#include "types.h"

struct Arm7Context;

struct ARM7Backend {

    static u32 DYNACALL singleOp(Arm7Context* ctx, u32 opcode);
    static u32 DYNACALL Step(Arm7Context* ctx);
    static u32 DYNACALL StepMany(Arm7Context* ctx, u32 minCycles);

    static void DYNACALL CPUSwitchMode(Arm7Context* ctx, int mode, bool saveState);
    static void DYNACALL CPUUpdateFlags(Arm7Context* ctx);
    static void DYNACALL UpdateInterrupts(Arm7Context* ctx);
    static void DYNACALL CPUUpdateCPSR(Arm7Context* ctx);
    static void DYNACALL CPUFiq(Arm7Context* ctx);

    virtual void Run(u32 uNumCycles) = 0;
    virtual void UpdateInterrupts() = 0;
    virtual void InvalidateJitCache() = 0;
    virtual void* GetEntrypointBase() = 0;

    virtual ~ARM7Backend() { }

    static ARM7Backend* CreateInterpreter(Arm7Context* ctx);
    static ARM7Backend* CreateJit(Arm7Context* ctx);
};

void libARM_SetResetState(bool Reset);
void libARM_InterruptChange(u32 bits, u32 L);

#define arm_sh4_bias (2)



