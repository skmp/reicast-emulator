#pragma once
#include "drkPvr.h"

bool spg_Init();
void spg_Term();
void spg_Reset(bool Manual);

void CalculateSync();
void read_lightgun_position(int x, int y);
struct TA_context;
void SetREP(TA_context* cntx);
