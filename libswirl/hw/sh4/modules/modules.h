#pragma once
#include "types.h"
#include "sh4_mod.h"

struct SuperH4Mmr;
struct SystemBus;

void bsc_init(SuperH4Mmr* sh4mmr);
void bsc_reset();
void bsc_term();

struct Sh4ModCpg : SuperH4Module {

    static Sh4ModCpg* Create(SuperH4Mmr* sh4mmr);
};

void dmac_init(SuperH4Mmr* sh4mmr, SystemBus* sb);
void dmac_reset();
void dmac_term();

struct Sh4ModRtc : SuperH4Module {
    
    static Sh4ModRtc* Create(SuperH4Mmr* sh4mmr);
};


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
