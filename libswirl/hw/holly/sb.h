/*
	This file is part of libswirl
*/
#include "license/bsd"


/*
	Nice helper #defines
*/

#pragma once
#include "types.h"
#include "hw/RegisterStruct.h"
#include "hw/StaticForward.h"
#include "hw/sh4/sh4_mmio.h"
#include <array>

struct SystemBus : MMIODevice {
	array<RegisterStruct, 0x540> sb_regs;

    virtual void RegisterRIO(void* context, u32 reg_addr, RegIO flags, RegReadAddrFP* rf = nullptr, RegWriteAddrFP* wf = nullptr) = 0;
};

#include "sb_regs.h"