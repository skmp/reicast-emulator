#pragma once
#include "types.h"
#include "hw/arm7/arm7.h"

extern u32 VREG;
extern VArray2 aica_ram;
u32 GetRTC_now();
u32 ReadMem_aica_rtc(u32 addr,u32 sz);
void WriteMem_aica_rtc(u32 addr,u32 data,u32 sz);
u32 ReadMem_aica_reg(u32 addr,u32 sz);
void WriteMem_aica_reg(u32 addr,u32 data,u32 sz);

void aica_Init();
void aica_Reset(bool Manual);
void aica_Term();

void aica_sb_Init();
void aica_sb_Reset(bool Manual);
void aica_sb_Term();

s32 libAICA_Init();
void libAICA_Reset(bool hard);
void libAICA_Term();
