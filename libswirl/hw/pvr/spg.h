#pragma once
#include "drkPvr.h"
#include "hw/sh4/sh4_mmio.h"

struct ASIC;
struct SPG : MMIODevice {
    
    virtual void CalculateSync() = 0;
    virtual void read_lightgun_position(int x, int y) = 0;
    
    virtual ~SPG() { }

    static SPG* Create(ASIC* asic);
};

#if 0
bool spg_Init();
void spg_Term();
void spg_Reset(bool Manual);

//#define Frame_Cycles (DCclock/60)

//need to replace 511 with correct value
//#define Line_Cycles (Frame_Cycles/511)

void spgUpdatePvr(u32 cycles);
bool spg_Init();
void spg_Term();
void spg_Reset(bool Manual);
void CalculateSync();
#endif

void read_lightgun_position(int x, int y);