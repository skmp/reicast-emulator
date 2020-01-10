#pragma once

#include "types.h"

#include "sh4_interpreter.h"

#include <memory>

enum Area0Hanlders {
    A0H_BIOS,
    A0H_FLASH,
    A0H_GDROM,
    A0H_SB,
    A0H_PVR,
    A0H_MODEM,
    A0H_AICA,
    A0H_RTC,
    A0H_EXT,

    A0H_MAX
};


struct SuperH4_impl final : SuperH4 {

    unique_ptr<MMIODevice> devices[A0H_MAX];

    void SetA0Handler(Area0Hanlders slot, MMIODevice* dev);

    bool setBackend(SuperH4Backends backend);

    void Run();

    void Stop();

    void Start();

    void Step();

    void Skip();

    void Reset(bool Manual);

    bool IsRunning();

    bool Init();

    void Term();

    void ResetCache();
};