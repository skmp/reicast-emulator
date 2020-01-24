#pragma once
#include "types.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mmio.h"

struct AicaContext {
	u8 regs[0x8000];
};

struct AudioStream;
struct SystemBus;
struct ASIC;
struct DSP;

struct AICA : MMIODevice {
	//Mainloop
	virtual void Update(u32 Samples) = 0;

	//Aica reads (both sh4&arm)
	virtual u32 ReadReg(u32 addr, u32 size) = 0;
	virtual void WriteReg(u32 addr, u32 data, u32 size) = 0;

	virtual void TimeStep() = 0;

	static AicaContext* CreateContext() {
		return new AicaContext();
	}

	static AICA* Create(AudioStream* audio_stream, SystemBus* sb, ASIC* asic, DSP* dsp, AicaContext* aica_ctx, u8* aica_ram, u32 aram_size);

	static MMIODevice* CreateRTC();
};

struct ASIC;
struct DSP;




void libAICA_TimeStep();

u32 libAICA_GetRTC_now();