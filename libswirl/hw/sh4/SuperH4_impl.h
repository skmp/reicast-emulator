#pragma once

#include "types.h"

#include "sh4_interpreter.h"

#include <memory>

struct SuperH4Mmr;

struct SuperH4_impl final : SuperH4 {
    unique_ptr<SuperH4Mmr> sh4mmr;
    unique_ptr<MMIODevice> devices[A0H_MAX];

    void SetA0Handler(Area0Hanlders slot, MMIODevice* dev);
    MMIODevice* GetA0Handler(Area0Hanlders slot);

    bool setBackend(SuperH4Backends backend);

    void Run();

    void Stop();

    void Start();

    void Step();

    void Skip();

    void Reset(bool Manual);

    bool IsRunning();

    bool Init(SystemBus* sb);

    void Term();

    void ResetCache();

    void serialize(void** data, unsigned int* total_size);
    void unserialize(void** data, unsigned int* total_size);
};