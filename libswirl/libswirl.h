#pragma once

#include "types.h"
#include "oslib/context.h"
#include <memory>

struct VirtualDreamcast {
    virtual void LoadState() = 0;
    virtual void SaveState() = 0;
    virtual void Stop() = 0;
    virtual void Exit() = 0;
    virtual void Reset() = 0;
    virtual void Resume() = 0;
    virtual bool Init() = 0;
    virtual void Term() = 0;
    virtual int StartGame(const char* path) = 0;
    virtual void RequestReset() = 0;
    virtual bool HandleFault(unat addr, rei_host_context_t* ctx) = 0;
    virtual ~VirtualDreamcast() { }

    static VirtualDreamcast* Create();
};

extern unique_ptr<VirtualDreamcast> virtualDreamcast;
extern unique_ptr<GDRomDisc> g_GDRDisc;

extern MMIODevice* g_GDRomDrive;


// TODO: rename these
int reicast_init(int argc, char* argv[]);
void reicast_ui_loop();
void reicast_term();