#pragma once
#include "aica.h"

extern VLockedMemory aica_ram;

extern void aica_init_mem();
extern void aica_term_mem();

extern u8 aica_reg[0x8000];

#define AICA_RAM_SIZE (ARAM_SIZE)
#define AICA_RAM_MASK (ARAM_MASK)