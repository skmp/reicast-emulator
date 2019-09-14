// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "LogManager.h"
#include "libretro-common/include/libretro.h"

class ConsoleListener : public LogListener
{
public:
  ConsoleListener(void *lob_cb);
  ~ConsoleListener();

  void Log(LogTypes::LOG_LEVELS, const char* text) override;

private:
  bool m_use_color;
  retro_log_printf_t retro_printf = nullptr;
};
