/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "hw/sh4/sh4_mmio.h"

enum DspBackends {
	DSPBE_INTERPRETER,
	DSPBE_DYNAREC,
};

struct dsp_context_t;
struct AicaContext;

struct DSP : MMIODevice {
	virtual void Step() = 0;
	virtual void WritenMem(u32 addr) = 0;
	virtual bool setBackend(DspBackends backend) = 0;
	virtual dsp_context_t* GetDspContext() = 0;

	static DSP* Create(AicaContext* aica_ctx, u8* aica_ram, u32 aram_size);
};

static inline void libDSP_Step() {
	sh4_cpu->GetA0H<DSP>(A0H_DSP)->Step();
}

