# Building with CMake (WIP)

## Android

Based on [https://developer.android.com/ndk/guides/cmake](https://developer.android.com/ndk/guides/cmake).

### Install

- Download then extract the Android NDK from [https://developer.android.com/ndk/downloads](https://developer.android.com/ndk/downloads).

### Build

    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
          -DANDROID_ABI=$ABI \
          -DANDROID_NATIVE_API_LEVEL=$MINSDKVERSION .
    cmake --build .

- `$NDK` = absolute path to the downloaded Android NDK.
- `$ABI` = a value from [https://developer.android.com/ndk/guides/abis](https://developer.android.com/ndk/guides/abis).
- `$MINSDKVERSION` = 16; Jelly Bean (4.1.x) and later are supported by Reicast.

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
