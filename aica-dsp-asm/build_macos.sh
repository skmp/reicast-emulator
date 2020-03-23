#!/bin/bash

clang++ -target x86_64-apple-macos10.9 -std=c++14 -stdlib=libc++ main.cpp macos_support.mm ../libswirl/hw/aica/dsp_helpers.cpp -I.. -I../libswirl -DTARGET_OSX_X64 -DHAVE_STDLIB_H -std=c++11 -framework Foundation -o aica-dsp-asm
