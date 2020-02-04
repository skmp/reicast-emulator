/*
	This file is part of libswirl
*/
#include "license/bsd"



#pragma once
#include <memory>
#include <functional>

#include "gui.h"

void HideOSD();

struct GUIRenderer {

    virtual void Start() = 0; // only sets the flag
    virtual void Stop() = 0;

    virtual void UILoop() = 0;

    virtual void QueueEmulatorFrame(std::function<bool()> callback) = 0;

    virtual void WaitQueueEmpty() = 0;

    virtual ~GUIRenderer() { }

    static GUIRenderer* Create(GUI* gui);
};

extern std::unique_ptr<GUIRenderer> g_GUIRenderer;