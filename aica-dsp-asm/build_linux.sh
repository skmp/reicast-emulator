#!/bin/bash

g++ main.cpp ../libswirl/hw/aica/dsp_helpers.cpp -I../libswirl -DTARGET_LINUX_x64 -o aica-dsp-asm
