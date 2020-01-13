# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Relative to reicast-emulator/reicast/android-studio/reicast/
LOCAL_PATH:= .

#lib swirl path
RZDCY_SRC_DIR := ../../../libswirl

include $(CLEAR_VARS)

CHD5_LZMA := 1
CHD5_FLAC := 1
USE_MODEM := 1
SCRIPTING := 1

PLATFORM_ANDROID := 1
SUPPORT_EGL := 1

LOCAL_CFLAGS := -O3 -D _ANDROID

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  ARM32_REC := 1
  LOCAL_CFLAGS += -D TARGET_LINUX_ARMELv7
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  ARM64_REC := 1
  LOCAL_CFLAGS += -D TARGET_LINUX_ARMv8
endif

ifeq ($(TARGET_ARCH_ABI),x86)
  X86_REC := 1
  LOCAL_CFLAGS += -D TARGET_LINUX_X86
endif

ifeq ($(TARGET_ARCH_ABI),mips)
  CPP_REC := 1
  LOCAL_CFLAGS += -D TARGET_LINUX_MIPS
endif

$(info $$TARGET_ARCH_ABI is [${TARGET_ARCH_ABI}])

include $(RZDCY_SRC_DIR)/core.mk

LOCAL_SRC_FILES := $(RZDCY_FILES)
LOCAL_CFLAGS    += $(RZDCY_CFLAGS) -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections
LOCAL_CPPFLAGS  += -fPIC -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections

# 7-Zip/LZMA settings (CHDv5)
ifdef CHD5_LZMA
	LOCAL_CFLAGS += -D_7ZIP_ST -DCHD5_LZMA
endif

# FLAC settings (CHDv5)
ifdef CHD5_FLAC
	LOCAL_CFLAGS += -DCHD5_FLAC
endif

ifdef NAOMI
    LOCAL_CFLAGS += -DTARGET_NAOMI=1
endif

LOCAL_CFLAGS +=

ifeq ($(TARGET_ARCH_ABI),x86)
  LOCAL_CFLAGS+= -DTARGET_NO_AREC -DTARGET_NO_OPENMP
endif

# LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_PRELINK_MODULE  := false

LOCAL_MODULE	:= dc
LOCAL_DISABLE_FORMAT_STRING_CHECKS=true
LOCAL_ASFLAGS += -fPIC -fvisibility=hidden
LOCAL_LDLIBS  += -llog -lEGL -lz -landroid  -fopenmp
#-Wl,-Map,./res/raw/syms.mp3
LOCAL_ARM_MODE	:= arm

ifeq ($(TARGET_ARCH_ABI),mips)
  LOCAL_LDFLAGS += -Wl,--gc-sections
else
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_LDFLAGS += -Wl,--gc-sections
  else
    LOCAL_LDFLAGS += -Wl,--gc-sections,--icf=safe
    LOCAL_LDLIBS +=  -Wl,--no-warn-shared-textrel
  endif
endif

$(LOCAL_SRC_FILES): $(VERSION_HEADER)

include $(BUILD_SHARED_LIBRARY)
