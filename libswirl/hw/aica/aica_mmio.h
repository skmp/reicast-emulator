#pragma once
#include "types.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mmio.h"

extern u32 VREG;
extern u32 RealTimeClock;

extern VLockedMemory aica_ram;

u32 ReadMem_aica_rtc(u32 addr,u32 sz);
void WriteMem_aica_rtc(u32 addr,u32 data,u32 sz);

struct AICA : MMIODevice {
	//Mainloop
	virtual void Update(u32 Samples) = 0;

	//Aica reads (both sh4&arm)
	virtual u32 ReadReg(u32 addr, u32 size) = 0;
	virtual void WriteReg(u32 addr, u32 data, u32 size) = 0;
};

u32 libAICA_ReadReg(u32 addr, u32 sz);
void libAICA_WriteReg(u32 addr, u32 data, u32 sz);