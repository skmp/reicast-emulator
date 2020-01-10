#pragma once
#include "types.h"
#include "hw/holly/sb.h"

extern u32 VREG;
extern u32 RealTimeClock;

u32 ReadMem_aica_rtc(u32 addr,u32 sz);
void WriteMem_aica_rtc(u32 addr,u32 data,u32 sz);

void aica_sb_Init(SBDevice* sb);
void aica_sb_Reset(bool Manual);
void aica_sb_Term();

// Used for dreamcast second setup
void aica_mmio_Init();
void aica_mmio_Reset(bool Manual);
void aica_mmio_Term();


// used internally by aica.cpp
u32 libAICA_ReadReg(u32 addr, u32 size);
void libAICA_WriteReg(u32 addr, u32 data, u32 size);