#pragma once
#include "types.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mmio.h"

struct AICA : MMIODevice {
	//Mainloop
	virtual void Update(u32 Samples) = 0;

	//Aica reads (both sh4&arm)
	virtual u32 ReadReg(u32 addr, u32 size) = 0;
	virtual void WriteReg(u32 addr, u32 data, u32 size) = 0;

	virtual void TimeStep() = 0;
};

struct ASIC;
struct DSP;

AICA* Create_AicaDevice(SystemBus* sb, ASIC* asic, DSP* dsp, u8* aica_ram);

MMIODevice* Create_RTCDevice();

u32 libAICA_ReadReg(u32 addr, u32 sz);
void libAICA_WriteReg(u32 addr, u32 data, u32 sz);
void libAICA_TimeStep();

u32 libAICA_GetRTC_now();