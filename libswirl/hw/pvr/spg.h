#pragma once

#include "hw/sh4/sh4_mmio.h"

struct ASIC;
struct SPG : MMIODevice {
    
    virtual void CalculateSync() = 0;
    virtual void read_lightgun_position(int x, int y) = 0;
    
    virtual ~SPG() { }

    static SPG* Create(ASIC* asic);
};

void read_lightgun_position(int x, int y);