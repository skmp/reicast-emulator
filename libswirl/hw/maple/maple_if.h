#pragma once
#include "types.h"
#include "types.h"
#include "maple_devs.h"

extern maple_device* MapleDevices[4][6];

struct SystemBus;
MMIODevice* Create_MapleDevice(SystemBus* sb);

void maple_ReconnectDevices();

void maple_vblank();
