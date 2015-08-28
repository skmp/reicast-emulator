#pragma once
#include "types.h"

extern int dc_get_settings(settings_t* dc_settings, int argc, wchar* argv[]);
extern int dc_init(int argc, wchar* argv[]);
extern int dc_init(settings_t dc_settings);
extern void dc_run();
extern void dc_term();
