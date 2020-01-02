#pragma once
#include "types.h"

struct SBDevice;

//Init/Term , global
void pvr_sb_Init(SBDevice* sb);
void pvr_sb_Term();
//Reset -> Reset - Initialise
void pvr_sb_Reset(bool Manual);