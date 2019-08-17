# Building with CMake (WIP)

## Nintendo Switch

This guide has been written using [devkitA64 release 13](https://github.com/devkitPro/buildscripts/releases/tag/devkitA64_r13).

A similar procedure can be done to build for other Nintendo platforms.

### Install

- Follow the install instructions from [https://devkitpro.org/wiki/devkitPro_pacman](https://devkitpro.org/wiki/devkitPro_pacman) to setup `(dkp-)pacman` then :


    sudo (dkp-)pacman -Sy
    sudo (dkp-)pacman -S devkitpro-pkgbuild-helpers switch-dev switch-pkg-config

### Build

    source /opt/devkitpro/switchvars.sh
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/switch.cmake .
    cmake --build .
