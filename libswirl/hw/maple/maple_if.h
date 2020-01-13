#pragma once
#include "types.h"
#include "types.h"
#include "maple_devs.h"

extern maple_device* MapleDevices[4][6];

struct SystemBus;
struct ASIC;
MMIODevice* Create_MapleDevice(SystemBus* sb, ASIC* asic);

void maple_ReconnectDevices();

void maple_vblank();
