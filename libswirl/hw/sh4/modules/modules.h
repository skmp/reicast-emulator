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

void intc_init(SuperH4Mmr* sh4mmr);
void intc_reset();
void intc_term();

void serial_init(SuperH4Mmr* sh4mmr);
void serial_reset();
void serial_term();

void ubc_init(SuperH4Mmr* sh4mmr);
void ubc_reset();
void ubc_term();

void tmu_init(SuperH4Mmr* sh4mmr);
void tmu_reset();
void tmu_term();

void ccn_init(SuperH4Mmr* sh4mmr);
void ccn_reset();
void ccn_term();

void MMU_init();
void MMU_reset();
void MMU_term();
