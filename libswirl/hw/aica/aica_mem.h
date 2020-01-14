#pragma once
#include "aica.h"

extern VLockedMemory aica_ram;

extern void aica_init_mem();
extern void aica_term_mem();

#define AICA_RAM_SIZE (ARAM_SIZE)
#define AICA_RAM_MASK (ARAM_MASK)