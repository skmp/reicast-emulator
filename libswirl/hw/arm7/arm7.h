#pragma once
#include "types.h"
struct ARM7Backend {
    virtual bool Init() = 0;
    virtual void Reset() = 0;

    virtual void Run(u32 uNumCycles) = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void InterruptChange(u32 bits, u32 L) = 0;

    virtual void serialize(void** data, unsigned int* total_size) = 0;
    virtual void unserialize(void** data, unsigned int* total_size) = 0;

    virtual ~ARM7Backend() { }
};

void libARM_SetResetState(bool Reset);
void libARM_InterruptChange(u32 bits, u32 L);

ARM7Backend* Create_ARM7Interpreter(u8* aica_ram);

#define arm_sh4_bias (2)



