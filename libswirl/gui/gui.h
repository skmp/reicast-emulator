/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <memory>

struct GUI {
    virtual void Init() = 0;
    virtual void OpenSettings() = 0;
    virtual void RenderUI() = 0;
    virtual void DisplayNotification(const char* msg, int duration) = 0;
    virtual void RenderOSD() = 0;
    virtual void OpenOnboarding() = 0;
    virtual void Term() = 0;
    virtual void RefreshFiles() = 0;

    virtual bool IsOpen() = 0;
    virtual bool IsVJoyEdit() = 0;
    virtual bool IsContentBrowser() = 0;

    static GUI* Create();
};

extern std::unique_ptr<GUI> g_GUI;