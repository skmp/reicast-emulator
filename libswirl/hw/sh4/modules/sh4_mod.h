#pragma once

struct SuperH4Module {

    virtual void Init() = 0;
    virtual void Reset() = 0;

    virtual void serialize(void** data, unsigned int* total_size) { }
    virtual void unserialize(void** data, unsigned int* total_size) { }

    virtual ~SuperH4Module() { }
};