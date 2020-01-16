#pragma once
#include "types.h"

struct Arm7Context;

struct ARM7Backend {

    static u32 Step(Arm7Context* ctx);
    static u32 StepMany(Arm7Context* ctx, u32 minCycles);

    static void UpdateInterrupts(Arm7Context* ctx);

    virtual void Run(u32 uNumCycles) = 0;
    virtual void UpdateInterrupts() = 0;
    virtual void InvalidateICache() { }

    virtual ~ARM7Backend() { }

    static ARM7Backend* CreateInterpreter(Arm7Context* ctx);
};

void libARM_SetResetState(bool Reset);
void libARM_InterruptChange(u32 bits, u32 L);

#define arm_sh4_bias (2)



