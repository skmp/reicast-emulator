#pragma once
#include <memory>
#include <functional>

#include "gui.h"

struct GUIRenderer {

    virtual void Stop() = 0;

    virtual void UILoop() = 0;

    virtual void QueueEmulatorFrame(std::function<bool(bool canceled)> callback) = 0;

    virtual void FlushEmulatorQueue() = 0;

    virtual ~GUIRenderer() { }

    static GUIRenderer* Create(GUI* gui);
};

extern std::unique_ptr<GUIRenderer> g_GUIRenderer;