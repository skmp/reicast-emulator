#!/bin/bash

clang++ -std=c++14 -stdlib=libc++ main.cpp macos_support.mm ../libswirl/hw/aica/dsp_helpers.cpp -I../libswirl -DTARGET_OSX_X64 -framework Foundation -o aica-dsp-asm
