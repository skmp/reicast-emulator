DEBUG         := 0
NO_REND       := 0
HAVE_GL       := 1
HAVE_OIT      := 0
HAVE_CORE     := 0
NO_THREADS    := 1
NO_EXCEPTIONS := 0
NO_NVMEM      := 0
NO_VERIFY     := 1
HAVE_LTCG     := 0
HAVE_GENERIC_JIT   := 1
HAVE_GL3      := 0
FORCE_GLES    := 0
STATIC_LINKING:= 0
HAVE_TEXUPSCALE := 1
HAVE_OPENMP   := 1

ifeq ($(HAVE_OIT), 1)
TARGET_NAME   := reicast_oit
else
TARGET_NAME   := reicast
endif

CXX      = ${CC_PREFIX}g++
CC       = ${CC_PREFIX}gcc
CC_AS    = ${CC_PREFIX}as

MFLAGS   := 
ASFLAGS  := 
LDFLAGS  :=
LDFLAGS_END :=
INCFLAGS :=
LIBS     :=
CFLAGS   := 
CXXFLAGS :=

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
   CXXFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

UNAME=$(shell uname -a)

LIBRETRO_DIR := .

# Cross compile ?

ifeq (,$(ARCH))
	ARCH = $(shell uname -m)
endif

# Target Dynarec
WITH_DYNAREC = $(ARCH)

ifeq ($(ARCH), $(filter $(ARCH), i386 i686))
	WITH_DYNAREC = x86
endif

ifeq ($(platform),)
	platform = unix
	ifeq ($(UNAME),)
		platform = win
	else ifneq ($(findstring MINGW,$(UNAME)),)
		platform = win
	else ifneq ($(findstring Darwin,$(UNAME)),)
		platform = osx
	else ifneq ($(findstring win,$(UNAME)),)
		platform = win
	endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
	EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
	arch = intel
	ifeq ($(shell uname -p),powerpc)
		arch = ppc
	endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif

CORE_DIR := core

DYNAREC_USED = 0
CORE_DEFINES   := -D__LIBRETRO__

ifeq ($(NO_VERIFY),1)
CORE_DEFINES += -DNO_VERIFY
endif

DC_PLATFORM=dreamcast

HOST_CPU_X86=0x20000001
HOST_CPU_ARM=0x20000002
HOST_CPU_MIPS=0x20000003
HOST_CPU_X64=0x20000004

ifeq ($(STATIC_LINKING),1)
EXT=a

endif

# Unix
ifneq (,$(findstring unix,$(platform)))
	EXT    ?= so
   TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	LDFLAGS +=  -Wl,--no-undefined
ifneq (,$(findstring Haiku,$(shell uname -s)))
	LIBS += -lroot
else
	LIBS += -lrt
endif
ifneq ($(HAVE_OIT), 1)
	HAVE_GL3 = 1
endif
	fpic = -fPIC

	ifeq ($(WITH_DYNAREC), $(filter $(WITH_DYNAREC), x86_64 x64))
		CFLAGS += -DTARGET_LINUX_x64 -D TARGET_NO_AREC
		SINGLE_PREC_FLAGS=1
		CXXFLAGS += -fexceptions
		HAVE_GENERIC_JIT   = 0
	else ifeq ($(WITH_DYNAREC), x86)
		CFLAGS += -m32 -D TARGET_LINUX_x86
		SINGLE_PREC_FLAGS=1
		CXXFLAGS += -fno-exceptions
		MFLAGS += -m32
		ASFLAGS += --32
		LDFLAGS += -m32
		HAVE_GENERIC_JIT   = 0
	endif

	PLATFORM_EXT := unix

# Raspberry Pi
else ifneq (,$(findstring rpi,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	LIBS += -lrt
	ARM_FLOAT_ABI_HARD = 1
	SINGLE_PREC_FLAGS = 1
	
	ifeq (,$(findstring mesa,$(platform)))
		GLES = 1
		GL_LIB := -L/opt/vc/lib -lbrcmGLESv2
		INCFLAGS += -I/opt/vc/include
		CFLAGS += -DTARGET_NO_STENCIL
	else
		FORCE_GLES = 1
	endif

	ifneq (,$(findstring rpi2,$(platform)))
		CFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
		CXXFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	else ifneq (,$(findstring rpi3,$(platform)))
		CFLAGS += -march=armv8-a+crc -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard
		CXXFLAGS += -march=armv8-a+crc -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard
	endif

	PLATFORM_EXT := unix
	WITH_DYNAREC=arm

# ODROIDs
else ifneq (,$(findstring odroid,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	ifeq ($(BOARD),1)
		BOARD := $(shell cat /proc/cpuinfo | grep -i odroid | awk '{print $$3}')
	endif
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	LIBS += -lrt
	ARM_FLOAT_ABI_HARD = 1
	FORCE_GLES = 1
	SINGLE_PREC_FLAGS = 1

	CFLAGS += -marm -mfloat-abi=hard -mfpu=neon
	CXXFLAGS += -marm -mfloat-abi=hard -mfpu=neon

	ifneq (,$(findstring ODROIDC,$(BOARD)))
		# ODROID-C1
		CFLAGS += -mcpu=cortex-a5
		CXXFLAGS += -mcpu=cortex-a5
	else ifneq (,$(findstring ODROID-XU3,$(BOARD)))
		# ODROID-XU3 & -XU3 Lite
		ifeq "$(shell expr `gcc -dumpversion` \>= 4.9)" "1"
			CFLAGS += -march=armv7ve -mcpu=cortex-a15.cortex-a7
			CXXFLAGS += -march=armv7ve -mcpu=cortex-a15.cortex-a7
		else
			CFLAGS += -mcpu=cortex-a9
			CXXFLAGS += -mcpu=cortex-a9
		endif
	else
		# ODROID-U2, -U3, -X & -X2
		CFLAGS += -mcpu=cortex-a9
		CXXFLAGS += -mcpu=cortex-a9
	endif

	#Since we are using GCC, we use the CFLAGS and we add some extra parameters to be able to compile (taken from reicast/reicast-emulator)
	ASFLAGS += $(CFLAGS) -c  -frename-registers -fno-strict-aliasing -ffast-math -ftree-vectorize

	PLATFORM_EXT := unix
	WITH_DYNAREC=arm

# i.MX6
else ifneq (,$(findstring imx6,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	FORCE_GLES = 1
	LIBS += -lrt
	CPUFLAGS += -DNO_ASM
	PLATFORM_EXT := unix
	WITH_DYNAREC=arm

# OS X
else ifneq (,$(findstring osx,$(platform)))
	EXT    ?= dylib
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED += -dynamiclib
	OSXVER = `sw_vers -productVersion | cut -d. -f 2`
	OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
        fpic += -mmacosx-version-min=10.7
	LDFLAGS += -stdlib=libc++
	fpic = -fPIC
	CFLAGS += -D TARGET_NO_AREC
	SINGLE_PREC_FLAGS=1
	PLATCFLAGS += -D__MACOSX__ -DOSX
	GL_LIB := -framework OpenGL
	PLATFORM_EXT := unix
	# Target Dynarec
	ifeq ($(ARCH), $(filter $(ARCH), ppc))
		WITH_DYNAREC =
        else
                HAVE_GENERIC_JIT   = 0
	endif
	HAVE_OPENMP=0

# iOS
else ifneq (,$(findstring ios,$(platform)))
	EXT    ?= dylib
	ifeq ($(IOSSDK),)
		IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
	endif

	TARGET := $(TARGET_NAME)_libretro_ios.$(EXT)
	DEFINES += -DIOS
	GLES = 1
	WITH_DYNAREC=
	PLATFORM_EXT := unix
	#HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_ARM)

	PLATCFLAGS += -DHAVE_POSIX_MEMALIGN -DNO_ASM
	PLATCFLAGS += -DIOS -marm
	CPUFLAGS += -DNO_ASM  -DARM -D__arm__ -DARM_ASM -D__NEON_OPT
	CPUFLAGS += -marm -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp
	SHARED += -dynamiclib

	fpic = -fPIC
	GL_LIB := -framework OpenGLES

	CC = clang -arch armv7 -isysroot $(IOSSDK)
	CC_AS = perl ./tools/gas-preprocessor.pl $(CC)
	CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
	ifeq ($(platform),ios9)
		CC         += -miphoneos-version-min=8.0
		CC_AS      += -miphoneos-version-min=8.0
		CXX        += -miphoneos-version-min=8.0
		PLATCFLAGS += -miphoneos-version-min=8.0
	else
		CC += -miphoneos-version-min=5.0
		CC_AS += -miphoneos-version-min=5.0
		CXX += -miphoneos-version-min=5.0
		PLATCFLAGS += -miphoneos-version-min=5.0
	endif
	HAVE_OPENMP=0

# Theos iOS
else ifneq (,$(findstring theos_ios,$(platform)))
	DEPLOYMENT_IOSVERSION = 5.0
	TARGET = iphone:latest:$(DEPLOYMENT_IOSVERSION)
	ARCHS = armv7
	TARGET_IPHONEOS_DEPLOYMENT_VERSION=$(DEPLOYMENT_IOSVERSION)
	THEOS_BUILD_DIR := objs
	include $(THEOS)/makefiles/common.mk

	LIBRARY_NAME = $(TARGET_NAME)_libretro_ios
	DEFINES += -DIOS
	FORCE_GLES = 1
	WITH_DYNAREC=arm

	PLATCFLAGS += -DHAVE_POSIX_MEMALIGN -DNO_ASM
	PLATCFLAGS += -DIOS -marm
	CPUFLAGS += -DNO_ASM  -DARM -D__arm__ -DARM_ASM -D__NEON_OPT -DNOSSE


# Android
else ifneq (,$(findstring android,$(platform)))
	fpic = -fPIC
	EXT       ?= so
	TARGET := $(TARGET_NAME)_libretro_android.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common

	CC = arm-linux-androideabi-gcc
	CXX = arm-linux-androideabi-g++
	WITH_DYNAREC=arm
	FORCE_GLES = 1
	PLATCFLAGS += -DANDROID
	CPUCFLAGS  += -DNO_ASM
	CPUFLAGS += -marm -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -D__arm__ -DARM_ASM -D__NEON_OPT
	CFLAGS += -DANDROID

	PLATFORM_EXT := unix

# QNX
else ifeq ($(platform), qnx)
	fpic = -fPIC
	EXT       ?= so
	TARGET := $(TARGET_NAME)_libretro_$(platform).$(EXT)
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common

	CC = qcc -Vgcc_ntoarmv7le
	CC_AS = qcc -Vgcc_ntoarmv7le
	CXX = QCC -Vgcc_ntoarmv7le
	AR = QCC -Vgcc_ntoarmv7le
	WITH_DYNAREC=arm
	FORCE_GLES = 1
	PLATCFLAGS += -DNO_ASM -D__BLACKBERRY_QNX__
	CPUFLAGS += -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp -D__arm__ -DARM_ASM -D__NEON_OPT
	CFLAGS += -D__QNX__

	PLATFORM_EXT := unix

# ARM
else ifneq (,$(findstring armv,$(platform)))
	EXT       ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
	fpic := -fPIC
	CPUFLAGS += -DNO_ASM -DARM -D__arm__ -DARM_ASM -DNOSSE
	WITH_DYNAREC=arm
	PLATCFLAGS += -DARM
	ifneq (,$(findstring gles,$(platform)))
		FORCE_GLES = 1
	endif
	ifneq (,$(findstring cortexa5,$(platform)))
		CPUFLAGS += -marm -mcpu=cortex-a5
	else ifneq (,$(findstring cortexa8,$(platform)))
		CPUFLAGS += -marm -mcpu=cortex-a8
	else ifneq (,$(findstring cortexa9,$(platform)))
		CPUFLAGS += -marm -mcpu=cortex-a9
	else ifneq (,$(findstring cortexa15a7,$(platform)))
		CPUFLAGS += -marm -mcpu=cortex-a15.cortex-a7
	else
		CPUFLAGS += -marm
	endif
	ifneq (,$(findstring neon,$(platform)))
		CPUFLAGS += -D__NEON_OPT -mfpu=neon
	endif
	ifneq (,$(findstring softfloat,$(platform)))
		CPUFLAGS += -mfloat-abi=softfp
	else ifneq (,$(findstring hardfloat,$(platform)))
		CPUFLAGS += -mfloat-abi=hard
	endif

# emscripten
else ifeq ($(platform), emscripten)
	EXT       ?= bc
	TARGET := $(TARGET_NAME)_libretro_$(platform).$(EXT)
	FORCE_GLES := 1
	WITH_DYNAREC=
	CPUFLAGS += -Dasm=asmerror -D__asm__=asmerror -DNO_ASM -DNOSSE
	SINGLE_THREAD := 1
	PLATCFLAGS += -Drglgen_resolve_symbols_custom=reicast_rglgen_resolve_symbols_custom \
					  -Drglgen_resolve_symbols=reicast_rglgen_resolve_symbols

	NO_REC=0
	PLATFORM_EXT := unix
	#HAVE_SHARED_CONTEXT := 1

# Windows
else
ifneq ($(HAVE_OIT), 1)
	HAVE_GL3 = 1
endif
	EXT       ?= dll
	HAVE_GENERIC_JIT   = 0
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	LDFLAGS += -shared -static-libgcc -static-libstdc++ -Wl,--version-script=link.T -lwinmm -lgdi32
	GL_LIB := -lopengl32
	PLATFORM_EXT := win32
	SINGLE_PREC_FLAGS=1
	CC = gcc
	CXX = g++
ifeq ($(WITH_DYNAREC), x86)
	LDFLAGS += -m32
	CFLAGS += -m32
else
	CFLAGS += -D TARGET_NO_AREC
endif

endif

ifeq ($(STATIC_LINKING),1)
	fpic=
	SHARED=
endif

ifeq ($(SINGLE_PREC_FLAGS),1)
	CORE_DEFINES += -fno-builtin-sqrtf
endif

ifeq ($(ARMV7A_FLAGS),1)
	MFLAGS += -marm -march=armv7-a
	ASFLAGS += -march=armv7-a
endif

ifeq ($(ARMV7_CORTEX_A9_FLAGS),1)
	MFLAGS += -mcpu=cortex-a9
endif

ifeq ($(ARM_FLOAT_ABI_HARD),1)
	MFLAGS += -mfloat-abi=hard
	ASFLAGS += -mfloat-abi=hard
	CFLAGS += -DARM_HARDFP
endif

ifeq ($(WITH_DYNAREC), $(filter $(WITH_DYNAREC), x86_64 x64))
	HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_X64)
endif

ifeq ($(WITH_DYNAREC), x86)
	HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_X86)
endif

ifeq ($(WITH_DYNAREC), arm)
	HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_ARM)
endif

ifeq ($(WITH_DYNAREC), mips)
	HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_MIPS)
endif

ifeq ($(FORCE_GLES),1)
	GLES = 1
	GL_LIB := -lGLESv2
else ifneq (,$(findstring gles,$(platform)))
	GLES = 1
	GL_LIB := -lGLESv2
else ifeq ($(platform), win)
	GL_LIB := -lopengl32
else ifneq (,$(findstring osx,$(platform)))
	GL_LIB := -framework OpenGL
else ifneq (,$(findstring ios,$(platform)))
	GL_LIB := -framework OpenGLES
else ifeq ($(GL_LIB),)
	GL_LIB := -lGL
endif

CFLAGS       += $(HOST_CPU_FLAGS)
CXXFLAGS     += $(HOST_CPU_FLAGS)
RZDCY_CFLAGS += $(HOST_CPU_FLAGS)

include Makefile.common

ifeq ($(WITH_DYNAREC), x86)
	HAVE_LTCG = 0
endif

ifeq ($(DEBUG),1)
	OPTFLAGS       := -O0
	LDFLAGS        += -g
	CFLAGS         += -g
else
ifneq (,$(findstring msvc,$(platform)))
	OPTFLAGS       := -O2
else
	OPTFLAGS       := -O3
endif
	CORE_DEFINES   += -DNDEBUG
	LDFLAGS        += -DNDEBUG


ifeq ($(HAVE_LTCG), 1)
	CORE_DEFINES   += -flto
endif
	CORE_DEFINES      += -DRELEASE
endif


ifeq ($(HAVE_GL3), 1)
	HAVE_CORE = 1
	CORE_DEFINES += -DHAVE_GL3
endif


RZDCY_CFLAGS	+= $(CFLAGS) -c $(OPTFLAGS) -frename-registers -ffast-math -ftree-vectorize -fomit-frame-pointer 

ifeq ($(WITH_DYNAREC), arm)
	RZDCY_CFLAGS += -march=armv7-a -mcpu=cortex-a9 -mfpu=vfpv3-d16
	RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
else
	RZDCY_CFLAGS += -DTARGET_LINUX_x86
endif

ifeq ($(NO_THREADS),1)
	CORE_DEFINES += -DTARGET_NO_THREADS
else
	NEED_PTHREAD=1
endif

ifeq ($(NO_REC),1)
	CORE_DEFINES += -DTARGET_NO_REC
endif

ifeq ($(NO_REND),1)
	CORE_DEFINES += -DNO_REND=1
endif

ifeq ($(NO_EXCEPTIONS),1)
	CORE_DEFINES += -DTARGET_NO_EXCEPTIONS=1
endif

ifeq ($(NO_NVMEM),1)
	CORE_DEFINES += -DTARGET_NO_NVMEM=1
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fexceptions -fno-rtti -std=gnu++11

ifeq (,$(findstring msvc,$(platform)))
CORE_DEFINES   += -funroll-loops
endif

ifeq ($(HAVE_OIT), 1)
HAVE_CORE = 1
CORE_DEFINES += -DHAVE_OIT -DHAVE_GL4
endif

ifeq ($(HAVE_CORE), 1)
	CORE_DEFINES += -DCORE
endif

ifeq ($(HAVE_TEXUPSCALE), 1)
	CORE_DEFINES += -DHAVE_TEXUPSCALE
ifeq ($(HAVE_OPENMP), 1)
	CFLAGS += -fopenmp
	CXXFLAGS += -fopenmp
	LDFLAGS += -fopenmp
else
	CFLAGS += -DTARGET_NO_OPENMP
	CXXFLAGS += -DTARGET_NO_OPENMP
endif
ifeq ($(platform), win)
	LDFLAGS_END += -Wl,-Bstatic -lgmp -Wl,-Bstatic -lgomp 
endif
	NEED_CXX11=1
	NEED_PTHREAD=1
endif

ifeq ($(NEED_PTHREAD), 1)
	LIBS         += -lpthread
endif

ifeq ($(HAVE_GL), 1)
	ifeq ($(GLES),1)
		CORE_DEFINES += -DHAVE_OPENGLES -DHAVE_OPENGLES2
	else
		CORE_DEFINES += -DHAVE_OPENGL
	endif
endif

ifeq ($(DEBUG), 1)
	HAVE_GENERIC_JIT = 0
endif

ifeq ($(HAVE_GENERIC_JIT), 1)
	CORE_DEFINES += -DTARGET_NO_JIT
	NEED_CXX11=1
endif

ifeq ($(NEED_CXX11), 1)
	CXXFLAGS     += -std=c++11
endif

RZDCY_CFLAGS   += $(CORE_DEFINES)
RZDCY_CXXFLAGS += $(CORE_DEFINES)
CFLAGS         += $(CORE_DEFINES)
CXXFLAGS       += $(CORE_DEFINES)

CFLAGS   += $(OPTFLAGS) -c
CFLAGS   += -fno-strict-aliasing -ffast-math
CXXFLAGS += -fno-rtti -fpermissive -fno-operator-names
LIBS     += -lm 

PREFIX        ?= /usr/local

ifneq (,$(findstring arm, $(ARCH)))
	CC_AS    = ${CC_PREFIX}gcc #The ngen_arm.S must be compiled with gcc, not as
	ASFLAGS  += $(CFLAGS)
endif

ifeq ($(PGO_MAKE),1)
	CFLAGS += -fprofile-generate -pg
	LDFLAGS += -fprofile-generate
else
	CFLAGS += -fomit-frame-pointer
endif

ifeq ($(PGO_USE),1)
	CFLAGS += -fprofile-use
endif

ifeq ($(LTO_TEST),1)
	CFLAGS += -flto -fwhole-program 
	LDFLAGS +=-flto -fwhole-program 
endif

CFLAGS     += $(fpic)
CXXFLAGS   += $(fpic)
LDFLAGS    += $(fpic)

OBJECTS := $(SOURCES_CXX:.cpp=.o) $(SOURCES_C:.c=.o) $(SOURCES_ASM:.S=.o)

ifneq (,$(findstring msvc,$(platform)))
	OBJOUT = -Fo
	LINKOUT = -out:
	LD = link.exe
else
	LD = $(CXX)
endif

all: $(TARGET)	
$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(LD) $(MFLAGS) $(fpic) $(SHARED) $(LDFLAGS) $(OBJECTS) $(LDFLAGS_END) $(GL_LIB) $(LIBS) -o $@
endif

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) $(MFLAGS) $< -o $@

%.o: %.S
	$(CC_AS) $(ASFLAGS) $(INCFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

