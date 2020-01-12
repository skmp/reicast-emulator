#pragma once

#include "types.h"

struct MMIODevice
{
    virtual bool Init() { return true; }
    virtual void Reset(bool m) { }
    virtual void Term() { }

    virtual u32 Read(u32 addr, u32 sz) { die("not implemented"); return 0; };
    virtual void Write(u32 addr, u32 data, u32 sz) { die("not implemented"); };

    virtual ~MMIODevice() { }
};
