#pragma once
#include "hw/sh4/sh4_mmio.h"

enum Arm7Backends {
	ARM7BE_INTERPRETER,
	ARM7BE_DYNAREC
};

struct AICA;

struct SoundCPU : MMIODevice {
	
	virtual bool setBackend(Arm7Backends backend) = 0;

	virtual void SetResetState(u32 State) = 0;
	virtual void Update(u32 cycles) = 0;
	virtual void InterruptChange(u32 bits, u32 L) = 0;

	virtual ~SoundCPU() { }

	static SoundCPU* Create(AICA* aica, u8* aica_ram, u32 aram_size);
};