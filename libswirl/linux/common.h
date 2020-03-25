/*
	This file is part of libswirl
*/
#include "license/bsd"

#pragma once

extern "C" {
    void common_linux_setup();

    #if FEAT_HAS_SERIAL_TTY
    int pty_master;
    bool common_serial_pty_setup();
    #endif
}