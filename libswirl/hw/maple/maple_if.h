#pragma once
#include "types.h"
#include "types.h"
#include "maple_devs.h"

extern maple_device* MapleDevices[4][6];

struct SBDevice;
void maple_Init(SBDevice* sb);
void maple_Reset(bool Manual);
void maple_Term();
void maple_ReconnectDevices();

void maple_vblank();
