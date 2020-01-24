#pragma once
#include "types.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_mmio.h"

void asic_RaiseInterrupt(HollyInterruptID inter);
void asic_CancelInterrupt(HollyInterruptID inter);

struct ASIC: MMIODevice {
    virtual void RaiseInterrupt(HollyInterruptID inter) = 0;
    virtual void CancelInterrupt(HollyInterruptID inter) = 0;
};

ASIC* Create_ASIC(SystemBus* sb);