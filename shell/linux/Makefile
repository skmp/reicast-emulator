

LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
#NO_REC := 1
#NO_REND := 1
WEBUI :=1 
USE_ALSA := 1

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip
LD=${CC}

MFLAGS := 
ASFLAGS := 
LDFLAGS :=
INCS :=
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

$(info Platform: $(platform))

# Generic 32 bit x86 (a.k.a. i386/i486/i686)
ifneq (,$(findstring x86,$(platform)))
    X86_REC := 1
    NOT_ARM := 1
    USE_X11 := 1
    MFLAGS += -m32
    ASFLAGS += --32
    LDFLAGS += -m32
    CFLAGS += -m32 -D TARGET_LINUX_x86 -D TARGET_NO_AREC -fsingle-precision-constant
    CXXFLAGS += -fno-exceptions

# Generic 64 bit x86 (a.k.a. x64/AMD64/x86_64/Intel64/EM64T)
else ifneq (,$(findstring x64,$(platform)))
    X64_REC := 1
    NOT_ARM := 1
    USE_X11 := 1
    CFLAGS += -D TARGET_LINUX_x64 -D TARGET_NO_AREC -fsingle-precision-constant
    CXXFLAGS += -fexceptions

# Generic 32 bit ARMhf (a.k.a. ARMv7h)
else ifneq (,$(findstring armv7h,$(platform)))
    MFLAGS += -marm -mfloat-abi=hard -march=armv7-a -funroll-loops
    ASFLAGS += -mfloat-abi=hard -march=armv7-a
    ifneq (,$(findstring neon,$(platform)))
        MFLAGS += -mfpu=neon
        ASFLAGS += -mfpu=neon
    endif
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant
    USE_GLES := 1

# LinCPP
else ifneq (,$(findstring lincpp,$(platform)))
    CPP_REC := 1
    NOT_ARM := 1
    USE_X11 := 1
    CFLAGS += -D TARGET_LINUX_x64 -D TARGET_NO_JIT
    CXXFLAGS += -fexceptions -std=gnu++11

# Raspberry Pi 2
else ifneq (,$(findstring rpi2,$(platform)))
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
    ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant
    INCS += -I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I../linux-deps/include
    LIBS += -L/opt/vc/lib/  -L../linux-deps/lib -lbcm_host
    USE_GLES := 1

# BeagleBone Black
else ifneq (,$(findstring beagle,$(platform)))
    CC_PREFIX ?= arm-none-linux-gnueabi-
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
    ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE -fsingle-precision-constant
    USE_GLES := 1

# Pandora
else ifneq (,$(findstring pandora,$(platform)))
    FOR_PANDORA := 1
    USE_X11 := 1
    PGO_USE := 1
    USE_GLES := 1
    MFLAGS +== -marm -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -funroll-loops -fpermissive
    ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_PANDORA  -D WEIRD_SLOWNESS -fsingle-precision-constant

# ODROIDs
else ifneq (,$(findstring odroid,$(platform)))
    MFLAGS += -marm -mfpu=neon -mfloat-abi=hard -funroll-loops
    ASFLAGS += -mfpu=neon -mfloat-abi=hard
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP -fsingle-precision-constant
    USE_GLES := 1

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

RZDCY_SRC_DIR = ../../core

RZDCY_MODULES	:=	cfg/ \
						hw/arm7/ \
						hw/aica/ \
						hw/holly/ \
						hw/ \
						hw/gdrom/ \
						hw/maple/ \
						hw/mem/ \
						hw/pvr/ \
						hw/sh4/ \
						hw/sh4/interpr/ \
						hw/sh4/modules/ \
						plugins/ \
						profiler/ \
						oslib/ \
						hw/extdev/ \
						hw/arm/ \
						hw/naomi/ \
						imgread/ \
						linux/ \
						./ \
						deps/coreio/ \
						deps/zlib/ \
						deps/chdr/ \
						deps/crypto/ \
						deps/libelf/ \
						deps/chdpsr/ \
						arm_emitter/ \
						rend/ \
						reios/ \
						deps/libpng/

ifndef NO_REC
	RZDCY_MODULES += hw/sh4/dyna/
endif

ifndef NOT_ARM
    RZDCY_MODULES += rec-ARM/
endif

ifdef X86_REC
    RZDCY_MODULES += rec-x86/ emitter/
endif

ifdef X64_REC
    RZDCY_MODULES += rec-x64/
endif

ifdef CPP_REC
    RZDCY_MODULES += rec-cpp/
endif

ifndef NO_REND
    RZDCY_MODULES += rend/gles/
else
    RZDCY_MODULES += rend/norend/
endif

RZDCY_MODULES += linux-dist/

RZDCY_FILES := $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cpp))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.c))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.S))
	
RZDCY_CFLAGS	:= $(CFLAGS) -c -O3 -DRELEASE -frename-registers -fsingle-precision-constant -ffast-math -ftree-vectorize -fomit-frame-pointer
	
	ifndef NOT_ARM
		RZDCY_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mfpu=vfpv3-d16
		RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
	else
	  ifndef ISMIPS
      RZDCY_CFLAGS += -DTARGET_LINUX_x86
		else
      RZDCY_CFLAGS += -DTARGET_LINUX_MIPS
		endif
	endif

ifdef NO_REC
  RZDCY_CFLAGS += -DTARGET_NO_REC
endif

ifdef USE_GLES
  RZDCY_CFLAGS += -DGLES
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11

LDFLAGS += -g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common

CFLAGS += -g -O3 -D RELEASE -c #-D NO_REND
CFLAGS += -frename-registers -fno-strict-aliasing
CFLAGS += -ffast-math -ftree-vectorize

CXXFLAGS += -fno-rtti -fpermissive -fno-operator-names

INCS += -I$(RZDCY_SRC_DIR) \
		  -I$(RZDCY_SRC_DIR)/deps \
		  -I$(RZDCY_SRC_DIR)/khronos \
		  -I$(RZDCY_SRC_DIR)/libretro-common/include  \
		  -I$(RZDCY_SRC_DIR)/linux-dist/libco

LIBS += -lm -lrt -lpthread

RZDCY_FILES += ../../core/linux-dist/libco/libco.c
RZDCY_FILES += ../../core/libretro-common/memmap/memalign.c

PREFIX ?= /usr/local
MAN_DIR ?= ${PREFIX}/share/man/man1
MENUENTRY_DIR ?= ${PREFIX}/share/applications
ICON_DIR ?= ${PREFIX}/share/pixmaps

ifndef NOT_ARM
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

ifdef USE_X11
    CFLAGS 	 += -D SUPPORT_X11
    CXXFLAGS += -D SUPPORT_X11 
    LIBS     += -lX11
endif

ifdef USE_ALSA
    CXXFLAGS += -D USE_ALSA
    LIBS += -lasound
endif

ifdef USE_PULSEAUDIO
    CXXFLAGS += -D USE_PULSEAUDIO
    LIBS += -lpulse-simple
endif

# GLES on x11?
ifdef USE_GLES
    CXXFLAGS += -DGLES
    LIBS += -lEGL -lGLESv2
else
    LIBS += -ldl -lGL #for desktop gl
endif

EXECUTABLE_STRIPPED=nosym-reicast.elf
ifdef NAOMI
    CFLAGS += -D TARGET_NAOMI
    DC_PLATFORM=naomi
    EXECUTABLE=reicast_naomi.elf
    EXECUTABLE_NAME=reicast-naomi
else
    DC_PLATFORM=dreamcast
    EXECUTABLE=reicast.elf
    EXECUTABLE_NAME=reicast
endif

OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj-$(DC_PLATFORM)-$(platform)/%,$(OBJECTS))

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
	
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	cp $< $@ && $(STRIP) $@

obj-$(DC_PLATFORM)-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
obj-$(DC_PLATFORM)-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $< -o $@

obj-$(DC_PLATFORM)-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(EXECUTABLE_STRIPPED) .map
