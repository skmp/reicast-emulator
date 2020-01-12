#pragma once
#include "types.h"

void asic_RaiseInterrupt(HollyInterruptID inter);
void asic_CancelInterrupt(HollyInterruptID inter);

struct SystemBus;

//Init/Res/Term for regs
void asic_sb_Init(SystemBus* sb);
void asic_sb_Term();
void asic_sb_Reset(bool Manual);
