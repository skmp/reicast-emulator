

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

CORE_DIR := ../../core

SOURCES_CXX := $(CORE_DIR)/cfg/cfg.cpp \
					$(CORE_DIR)/cfg/ini.cpp \
					$(CORE_DIR)/cfg/cl.cpp \
					\
					$(CORE_DIR)/hw/arm7/arm_mem.cpp \
					$(CORE_DIR)/hw/arm7/virt_arm.cpp \
					$(CORE_DIR)/hw/arm7/arm7.cpp \
					$(CORE_DIR)/hw/arm7/vbaARM.cpp \
					\
					$(CORE_DIR)/hw/aica/aica_mem.cpp \
					$(CORE_DIR)/hw/aica/dsp.cpp \
					$(CORE_DIR)/hw/aica/aica.cpp \
					$(CORE_DIR)/hw/aica/sgc_if.cpp \
					$(CORE_DIR)/hw/aica/aica_if.cpp \
					\
					$(CORE_DIR)/hw/holly/sb.cpp \
					$(CORE_DIR)/hw/holly/sb_dma.cpp \
					$(CORE_DIR)/hw/holly/holly_intc.cpp \
					$(CORE_DIR)/hw/holly/sb_mem.cpp \
					\
					$(CORE_DIR)/hw/gdrom/gdrom_response.cpp \
					$(CORE_DIR)/hw/gdrom/gdromv3.cpp \
					\
					$(CORE_DIR)/hw/maple/maple_helper.cpp \
					$(CORE_DIR)/hw/maple/maple_devs.cpp \
					$(CORE_DIR)/hw/maple/maple_if.cpp \
					$(CORE_DIR)/hw/maple/maple_cfg.cpp \
					\
					$(CORE_DIR)/hw/mem/_vmem.cpp \
					\
					$(CORE_DIR)/hw/pvr/pvr_regs.cpp \
					$(CORE_DIR)/hw/pvr/Renderer_if.cpp \
					$(CORE_DIR)/hw/pvr/ta_ctx.cpp \
					$(CORE_DIR)/hw/pvr/ta_vtx.cpp \
					$(CORE_DIR)/hw/pvr/pvr_sb_regs.cpp \
					$(CORE_DIR)/hw/pvr/pvr_mem.cpp \
					$(CORE_DIR)/hw/pvr/spg.cpp \
					$(CORE_DIR)/hw/pvr/ta.cpp \
					$(CORE_DIR)/hw/pvr/drkPvr.cpp \
					\
					$(CORE_DIR)/hw/sh4/sh4_mmr.cpp \
					$(CORE_DIR)/hw/sh4/sh4_mem.cpp \
					$(CORE_DIR)/hw/sh4/sh4_interrupts.cpp \
					$(CORE_DIR)/hw/sh4/sh4_rom.cpp \
					$(CORE_DIR)/hw/sh4/sh4_core_regs.cpp \
					$(CORE_DIR)/hw/sh4/sh4_sched.cpp \
					$(CORE_DIR)/hw/sh4/sh4_opcode_list.cpp \
					$(CORE_DIR)/hw/sh4/interpr/sh4_interpreter.cpp \
					$(CORE_DIR)/hw/sh4/interpr/sh4_fpu.cpp \
					$(CORE_DIR)/hw/sh4/interpr/sh4_opcodes.cpp \
					$(CORE_DIR)/hw/sh4/modules/serial.cpp \
					$(CORE_DIR)/hw/sh4/modules/rtc.cpp  \
					$(CORE_DIR)/hw/sh4/modules/bsc.cpp \
					$(CORE_DIR)/hw/sh4/modules/tmu.cpp \
					$(CORE_DIR)/hw/sh4/modules/ccn.cpp \
					$(CORE_DIR)/hw/sh4/modules/intc.cpp \
					$(CORE_DIR)/hw/sh4/modules/ubc.cpp \
					$(CORE_DIR)/hw/sh4/modules/cpg.cpp \
					$(CORE_DIR)/hw/sh4/modules/dmac.cpp \
					$(CORE_DIR)/hw/sh4/modules/mmu.cpp \
					\
					$(CORE_DIR)/profiler/profiler.cpp \
					\
					$(CORE_DIR)/oslib/audiostream.cpp \
					\
					$(CORE_DIR)/imgread/ImgReader.cpp \
					$(CORE_DIR)/imgread/cdi.cpp \
					$(CORE_DIR)/imgread/ioctl.cpp \
					$(CORE_DIR)/imgread/chd.cpp \
					$(CORE_DIR)/imgread/common.cpp \
					$(CORE_DIR)/imgread/gdi.cpp \
					\
					$(CORE_DIR)/nullDC.cpp \
					$(CORE_DIR)/stdclass.cpp \
					\
					$(CORE_DIR)/deps/coreio/coreio.cpp \
					$(CORE_DIR)/deps/chdr/chdr.cpp \
					$(CORE_DIR)/deps/crypto/sha1.cpp \
					$(CORE_DIR)/deps/crypto/sha256.cpp \
					$(CORE_DIR)/deps/crypto/md5.cpp \
					$(CORE_DIR)/deps/libelf/elf.cpp \
					$(CORE_DIR)/deps/libelf/elf32.cpp \
					$(CORE_DIR)/deps/libelf/elf64.cpp \
					$(CORE_DIR)/deps/chdpsr/cdipsr.cpp \
					\
					$(CORE_DIR)/rend/TexCache.cpp \
					\
					$(CORE_DIR)/reios/reios_elf.cpp \
					$(CORE_DIR)/reios/reios.cpp \
					$(CORE_DIR)/reios/gdrom_hle.cpp \
					$(CORE_DIR)/reios/descrambl.cpp \
					\
					$(CORE_DIR)/rend/gles/gldraw.cpp \
					$(CORE_DIR)/rend/gles/gles.cpp \
					$(CORE_DIR)/rend/gles/gltex.cpp

# Dynarec

SOURCES_CXX += $(CORE_DIR)/hw/sh4/dyna/decoder.cpp \
					$(CORE_DIR)/hw/sh4/dyna/driver.cpp \
					$(CORE_DIR)/hw/sh4/dyna/blockmanager.cpp \
					$(CORE_DIR)/hw/sh4/dyna/shil.cpp 

# Recompiler (x64)

SOURCES_CXX += $(CORE_DIR)/rec-x64/rec_x64.cpp 

# Recompiler (ARM)
#
# SOURCES_CXX += $(CORE_DIR)/rec-ARM/rec-arm.cpp
# SOURCES_ASM += $(CORE_DIR)/rec-ARM/ngen_arm.S

# Recompiler (x86 32bit)
#
# SOURCES_CXX += $(CORE_DIR)/rec-x86/rec_x86_asm.cpp \
#					  $(CORE_DIR)/rec-x86/rec_x86_driver.cpp \
#					  $(CORE_DIR)/rec-x86/rec_x86_il.cpp
# SOURCES_ASM += $(CORE_DIR)/rec-x86/rec_lin86_asm.S

SOURCES_CXX += $(CORE_DIR)/linux-dist/main.cpp \
					$(CORE_DIR)/linux-dist/x11.cpp

SOURCES_CXX += $(CORE_DIR)/oslib/audiobackend_alsa.cpp \
					\
					$(CORE_DIR)/linux/common.cpp \
					$(CORE_DIR)/linux/context.cpp

SOURCES_C +=   $(CORE_DIR)/linux-dist/libco/libco.c \
					$(CORE_DIR)/libretro-common/memmap/memalign.c

SOURCES_C +=   $(CORE_DIR)/deps/zlib/deflate.c \
					$(CORE_DIR)/deps/zlib/gzlib.c \
					$(CORE_DIR)/deps/zlib/uncompr.c \
					$(CORE_DIR)/deps/zlib/zutil.c \
					$(CORE_DIR)/deps/zlib/inffast.c \
					$(CORE_DIR)/deps/zlib/gzread.c \
					$(CORE_DIR)/deps/zlib/crc32.c \
					$(CORE_DIR)/deps/zlib/gzwrite.c \
					$(CORE_DIR)/deps/zlib/inflate.c \
					$(CORE_DIR)/deps/zlib/infback.c \
					$(CORE_DIR)/deps/zlib/inftrees.c \
					$(CORE_DIR)/deps/zlib/trees.c \
					$(CORE_DIR)/deps/zlib/gzclose.c \
					$(CORE_DIR)/deps/zlib/compress.c \
					$(CORE_DIR)/deps/zlib/adler32.c

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

INCS     += -I$(CORE_DIR) \
				-I$(CORE_DIR)/deps \
				-I$(CORE_DIR)/khronos \
				-I$(CORE_DIR)/libretro-common/include  \
				-I$(CORE_DIR)/linux-dist/libco

LIBS    += -lm -lrt -lpthread

PREFIX        ?= /usr/local

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

OBJECTS := $(SOURCES_CXX:.cpp=.o) $(SOURCES_C:.c=.o) $(SOURCES_ASM:.S=.o)

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
	
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	cp $< $@ && $(STRIP) $@

%.o: %.cpp
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
%.o: %.c
	$(CC) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) $(INCS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(EXECUTABLE_STRIPPED) .map
