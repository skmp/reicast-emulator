#pragma once
#include "types.h"
#include "../arm7/arm7.h"

extern u32 VREG;
extern VArray2 aica_ram;
u32 aica_rtc_reg_read(u32 addr,u32 sz);
void aica_rtc_reg_write(u32 addr,u32 data,u32 sz);
u32 ReadMem_aica_reg(u32 addr,u32 sz);
void WriteMem_aica_reg(u32 addr,u32 data,u32 sz);

void aica_Init();
void aica_Reset(bool Manual);
void aica_Term();

#define arm_sh4_bias (2)

#define UpdateAica(clc) libAICA_Update(clc)
#define UpdateArm(clc) arm_Run(clc / arm_sh4_bias)

void aica_sb_Init();
void aica_sb_Reset(bool Manual);
void aica_sb_Term();
