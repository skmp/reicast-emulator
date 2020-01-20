#pragma once
#include "hw/sh4/sh4_mmio.h"

enum DspBackends {
	DSPBE_INTERPRETER,
	DSPBE_DYNAREC,
};

struct DSP : MMIODevice {
	virtual void Step() = 0;
	virtual void WritenMem(u32 addr) = 0;
	virtual bool setBackend(DspBackends backend) = 0;

	static DSP* Create(u8* aica_ram, u32 aram_size);
};

static void libDSP_Step() {
	sh4_cpu->GetA0H<DSP>(A0H_DSP)->Step();
}

