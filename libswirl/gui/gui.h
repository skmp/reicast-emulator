/*
	This file is part of libswirl
*/
#include "license/bsd"



#pragma once
#include <memory>
#include <functional>

struct GUI {
    virtual void Init() = 0;
    virtual void OpenSettings(std::function<void()> cb) = 0;
    virtual void RenderUI() = 0;
    virtual void DisplayNotification(const char* msg, int duration) = 0;
    virtual void RenderOSD() = 0;
    virtual void OpenOnboarding() = 0;
    virtual void RefreshFiles() = 0;

    virtual bool IsOpen() = 0;
    virtual bool IsVJoyEdit() = 0;
    virtual bool IsContentBrowser() = 0;
    virtual ~GUI() { }

    static GUI* Create();
};

extern std::unique_ptr<GUI> g_GUI;