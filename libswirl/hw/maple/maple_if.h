#pragma once
#include "types.h"
#include "types.h"
#include "maple_devs.h"

extern maple_device* MapleDevices[4][6];

struct SBDevice;
MMIODevice* Create_MapleDevice(SBDevice* sb);

void maple_ReconnectDevices();

void maple_vblank();
