CXX       = ${CC_PREFIX}g++
CC        = ${CC_PREFIX}gcc
CC_AS    ?= $(CC)
STRIP     = ${CC_PREFIX}strip
LD        = ${CC}

DEBUG    := 0
NO_REND  := 1

TARGET_NAME := reicast

MFLAGS   := 
ASFLAGS  := 
LDFLAGS  :=
INCFLAGS :=
LIBS :=
CFLAGS := 
CXXFLAGS :=

# Platform auto-detection
# Can be overridden by using:
#   make platform=x64
ifeq (,$(platform))
    ARCH = $(shell uname -m)
    ifeq ($(ARCH), $(filter $(ARCH), i386 i686))
        platform = x86
    else ifeq ($(ARCH), $(filter $(ARCH), x86_64 AMD64))
        platform = x64
    else ifneq (,$(findstring arm,$(ARCH)))
        HARDWARE = $(shell grep Hardware /proc/cpuinfo)
        ifneq (,$(findstring BCM2709,$(HARDWARE)))
            platform = rpi2
        else ifneq (,$(findstring AM33XX,$(HARDWARE)))
            platform = beagle
        else ifneq (,$(findstring Pandora,$(HARDWARE)))
            platform = pandora
        else ifneq (,$(findstring ODROIDC,$(HARDWARE)))
            platform = odroidc1
        else ifneq (,$(findstring ODROID-XU3,$(HARDWARE)))
            platform = odroidxu3
        else ifneq (,$(findstring ODROIDXU,$(HARDWARE)))
            platform = odroidxu
        else ifneq (,$(findstring ODROIDX2,$(HARDWARE)))
            platform = odroidx2
        else ifneq (,$(findstring ODROIDX,$(HARDWARE)))
            platform = odroidx
        else ifneq (,$(findstring ODROID-U2/U3,$(HARDWARE)))
            platform = odroidu2
        else ifneq (,$(findstring ODROIDU2,$(HARDWARE)))
            platform = odroidu2
        else
            platform = armv7h
        endif
    else
        $(error Unsupported CPU architecture)
    endif
endif

# Generic 32 bit x86 (a.k.a. i386/i486/i686)
ifneq (,$(findstring x86,$(platform)))
	 WITH_DYNAREC := x86
    MFLAGS += -m32
    ASFLAGS += --32
    LDFLAGS += -m32
    CFLAGS += -m32 -D TARGET_LINUX_x86 -D TARGET_NO_AREC -fsingle-precision-constant
    CXXFLAGS += -fno-exceptions

# Generic 64 bit x86 (a.k.a. x64/AMD64/x86_64/Intel64/EM64T)
else ifneq (,$(findstring x64,$(platform)))
	 WITH_DYNAREC := x64
    CFLAGS += -D TARGET_LINUX_x64 -D TARGET_NO_AREC -fsingle-precision-constant
    CXXFLAGS += -fexceptions

# Generic 32 bit ARMhf (a.k.a. ARMv7h)
else ifneq (,$(findstring armv7h,$(platform)))
	WITH_DYNAREC := arm
    MFLAGS += -marm -mfloat-abi=hard -march=armv7-a -funroll-loops
    ASFLAGS += -mfloat-abi=hard -march=armv7-a
    ifneq (,$(findstring neon,$(platform)))
        MFLAGS += -mfpu=neon
        ASFLAGS += -mfpu=neon
    endif
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant

# Raspberry Pi 2
else ifneq (,$(findstring rpi2,$(platform)))
	WITH_DYNAREC := arm
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
    ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant
    INCFLAGS += -I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I../linux-deps/include
    LIBS += -L/opt/vc/lib/  -L../linux-deps/lib -lbcm_host

# BeagleBone Black
else ifneq (,$(findstring beagle,$(platform)))
	WITH_DYNAREC := arm
    CC_PREFIX ?= arm-none-linux-gnueabi-
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
    ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE -fsingle-precision-constant

# ODROIDs
else ifneq (,$(findstring odroid,$(platform)))
	WITH_DYNAREC := arm
    MFLAGS += -marm -mfpu=neon -mfloat-abi=hard -funroll-loops
    ASFLAGS += -mfpu=neon -mfloat-abi=hard
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant

    # ODROID-XU3, -XU3 Lite & -XU4
    ifneq (,$(findstring odroidxu3,$(platform)))
        MFLAGS += -march=armv7ve -mtune=cortex-a15.cortex-a7
        ASFLAGS += -march=armv7ve

    # Other ODROIDs
    else
        MFLAGS += -march=armv7-a
        ASFLAGS += -march=armv7-a

        # ODROID-C1 & -C1+
        ifneq (,$(findstring odroidc1,$(platform)))
            MFLAGS += -mtune=cortex-a5

        # ODROID-U2, -U3, -X & -X2
        else
            MFLAGS += -mtune=cortex-a9

        endif
    endif
else
    $(error Unknown platform)
endif

CORE_DIR := ../../core

DYNAREC_USED = 0

include Makefile.common

ifeq ($(DEBUG),1)
OPTFLAGS       := -O0
else
OPTFLAGS       := -O3
endif
RZDCY_CFLAGS	:= $(CFLAGS) -c $(OPTFLAGS) -DRELEASE -frename-registers -fsingle-precision-constant -ffast-math -ftree-vectorize -fomit-frame-pointer -D__LIBRETRO__

ifeq ($(WITH_DYNAREC), arm)
RZDCY_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mfpu=vfpv3-d16
RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
else
RZDCY_CFLAGS += -DTARGET_LINUX_x86
endif

ifdef NO_REC
  RZDCY_CFLAGS += -DTARGET_NO_REC
endif

ifeq ($(NO_REND),1)
  RZDCY_CFLAGS += -DNO_REND=1
  CFLAGS 	 += -DNO_REND
  CXXFLAGS += -DNO_REND
endif

ifeq ($(USE_GLES),1)
  RZDCY_CFLAGS += -DGLES
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11

LDFLAGS += -g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,$(OPTFLAGS) -Wl,--sort-common

CFLAGS += -g $(OPTFLAGS) -D RELEASE -c
CFLAGS += -frename-registers -fno-strict-aliasing -ffast-math -ftree-vectorize

CXXFLAGS += -fno-rtti -fpermissive -fno-operator-names


LIBS    += -lm -lrt -lpthread

PREFIX        ?= /usr/local

ifeq ($(WITH_DYNAREC), arm)
else
    AS=${CC_PREFIX}gcc
    ASFLAGS += $(CFLAGS)
endif

ifdef PGO_MAKE
    CFLAGS += -fprofile-generate -pg
    LDFLAGS += -fprofile-generate
else
    CFLAGS += -fomit-frame-pointer
endif

ifdef PGO_USE
    CFLAGS += -fprofile-use
endif

ifdef LTO_TEST
    CFLAGS += -flto -fwhole-program 
    LDFLAGS +=-flto -fwhole-program 
endif

# GLES on x11?
ifdef USE_GL
ifdef USE_GLES
    CXXFLAGS += -DGLES
	 CFLAGS   += -DGLES
    GL_LIB   += -lEGL -lGLESv2
else
    GL_LIB += -ldl -lGL #for desktop gl
endif
endif

ifdef NAOMI
    CFLAGS += -D TARGET_NAOMI
    DC_PLATFORM=naomi
    TARGET_NAME=$(TARGET_NAME)_naomi
else
    DC_PLATFORM=dreamcast
endif

fpic := -fPIC
CFLAGS   += $(fpic)
CXXFLAGS += $(fpic)
SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined

OBJECTS := $(SOURCES_CXX:.cpp=.o) $(SOURCES_C:.c=.o) $(SOURCES_ASM:.S=.o)
TARGET  := $(TARGET_NAME)_libretro.so

all: $(TARGET)	
$(TARGET): $(OBJECTS)
	$(CXX) $(MFLAGS) $(fpic) $(SHARED) $(EXTRAFLAGS) $(LDFLAGS) $(GL_LIB) $(OBJECTS) $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(EXTRAFLAGS) $(INCFLAGS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
%.o: %.c
	$(CC) $(EXTRAFLAGS) $(INCFLAGS) $(CFLAGS) $< -o $@

%.o: %.S
	$(CC_AS) $(ASFLAGS) $(INCFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) .map
