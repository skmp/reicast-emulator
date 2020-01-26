#pragma once

#include "types.h"
#include <memory>

struct SuperH4Backend;
struct MMIODevice;

struct SuperH4_impl final : SuperH4 {
    unique_ptr<MMIODevice> devices[A0H_MAX];
    unique_ptr<SuperH4Backend> sh4_backend;

    void SetA0Handler(Area0Hanlders slot, MMIODevice* dev);
    MMIODevice* GetA0Handler(Area0Hanlders slot);

    bool setBackend(SuperH4Backends backend);

    SuperH4_impl();

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

    void serialize(void** data, unsigned int* total_size);
    void unserialize(void** data, unsigned int* total_size);
};