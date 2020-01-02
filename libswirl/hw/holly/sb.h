/*
	Nice helper #defines
*/

#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmio.h"

struct SBDevice : MMIODevice {
    virtual void RegisterRIO(u32 reg_addr, RegIO flags, RegReadAddrFP* rf = nullptr, RegWriteAddrFP * wf = nullptr) = 0;
};

extern Array<RegisterStruct> sb_regs;

#include "sb_regs.h"