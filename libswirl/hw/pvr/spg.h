#pragma once
#include "drkPvr.h"

/*
struct SPG {
    virtual bool spg_Init() = 0;
    virtual void spg_Term() = 0;
    virtual void spg_Reset(bool Manual) = 0;

    virtual void CalculateSync() = 0;
    virtual void read_lightgun_position(int x, int y) = 0;
    virtual void spgUpdatePvr(u32 cycles) = 0;

    virtual ~SPG() { }
};
*/

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
void read_lightgun_position(int x, int y);
