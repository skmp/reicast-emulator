#pragma once
#include "types.h"
#include "sh4_mod.h"

struct SuperH4Mmr;
struct SystemBus;

#define DEFAULT_MOD(Mod) \
    struct Sh4Mod##Mod : SuperH4Module { static Sh4Mod##Mod * Create(SuperH4Mmr* sh4mmr); };

DEFAULT_MOD(Bsc)
DEFAULT_MOD(Cpg)
DEFAULT_MOD(Dmac)
DEFAULT_MOD(Rtc)
DEFAULT_MOD(Intc)
DEFAULT_MOD(Serial)
DEFAULT_MOD(Ubc)
DEFAULT_MOD(Tmu)
DEFAULT_MOD(Ccn)

void MMU_init();
void MMU_reset();
void MMU_term();
