LOCAL_PATH := $(call my-dir)

CORE_DIR      := $(LOCAL_PATH)/..
ROOT_DIR      := $(LOCAL_PATH)/..
LIBRETRO_DIR  := $(ROOT_DIR)/libretro

SOURCES_C     :=
SOURCES_CXX   :=
SOURCES_ASM   :=
INCFLAGS      :=
CFLAGS        :=
CXXFLAGS      :=
DYNAFLAGS     :=
NO_THREADS    := 0
HAVE_NEON     := 0
WITH_DYNAREC  :=

HAVE_GL       := 1
HAVE_OPENGL   := 1
GLES          := 1

HOST_CPU_X86=0x20000001
HOST_CPU_ARM=0x20000002
HOST_CPU_MIPS=0x20000003
HOST_CPU_X64=0x20000004

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
  WITH_DYNAREC := arm64
  HAVE_GENERIC_JIT :=1
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
  WITH_DYNAREC := arm
  HAVE_NEON := 1
else ifeq ($(TARGET_ARCH_ABI),x86)
  # X86 dynarec isn't position independent, so it will not run on api 23+
  # This core uses vulkan which is api 24+, so dynarec cannot be used
  WITH_DYNAREC := bogus
else ifeq ($(TARGET_ARCH_ABI),x86_64)
  WITH_DYNAREC := x86_64
endif

include $(ROOT_DIR)/Makefile.common

COREFLAGS := -ffast-math -D__LIBRETRO__ -DINLINE="inline" -DANDROID -DHAVE_OPENGLES -DHAVE_OPENGLES2 $(GLFLAGS) $(INCFLAGS) $(DYNAFLAGS)

ifeq ($(NO_THREADS),1)
COREFLAGS += -DTARGET_NO_THREADS
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
COREFLAGS += -fno-operator-names
endif

ifeq ($(WITH_DYNAREC), $(filter $(WITH_DYNAREC), x86_64 x64))
	COREFLAGS += -DHOST_CPU=$(HOST_CPU_X64)
endif

ifeq ($(WITH_DYNAREC), x86)
	COREFLAGS += -DHOST_CPU=$(HOST_CPU_X86)
endif

ifeq ($(WITH_DYNAREC), arm)
	COREFLAGS += -DHOST_CPU=$(HOST_CPU_ARM)
endif

ifeq ($(WITH_DYNAREC), mips)
	COREFLAGS += -DHOST_CPU=$(HOST_CPU_MIPS)
endif

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C) $(SOURCES_ASM)
LOCAL_CFLAGS       := $(COREFLAGS) $(CFLAGS)
LOCAL_CXXFLAGS     := -std=c++11 $(COREFLAGS) $(CXXFLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/link.T
LOCAL_LDLIBS       := -lGLESv2
LOCAL_CPP_FEATURES := exceptions
LOCAL_ARM_NEON     := true
LOCAL_ARM_MODE     := arm

ifeq ($(NO_THREADS),1)
else
LOCAL_LDLIBS       += -lpthread
endif
include $(BUILD_SHARED_LIBRARY)
