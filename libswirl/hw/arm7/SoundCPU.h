#pragma once
#include "hw/sh4/sh4_mmio.h"
struct SoundCPU : MMIODevice {
	
	virtual void SetResetState(u32 State) = 0;
	virtual void Update(u32 cycles) = 0;
	
	virtual ~SoundCPU() { }

	static SoundCPU* Create();
};