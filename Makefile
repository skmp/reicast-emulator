DEBUG         := 0
NO_REND       := 0
HAVE_GL       := 1
HAVE_CORE     := 0
NO_THREADS    := 1
NO_EXCEPTIONS := 0
NO_NVMEM      := 0
NO_VERIFY     := 1
NAOMI         := 0
HAVE_GENERIC_JIT   := 1
FORCE_GLES    := 0
STATIC_LINKING:= 0

TARGET_NAME   := reicast

MFLAGS   := 
ASFLAGS  := 
LDFLAGS  :=
INCFLAGS :=
LIBS     :=
CFLAGS   := 
CXXFLAGS :=
CC_AS ?= $(AS)

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

ifeq ($(SINGLE_PREC_FLAGS),1)
CFLAGS += -fsingle-precision-constant
RZDCY_CFLAGS += -fsingle-precision-constant
endif

ifeq ($(ARMV7A_FLAGS),1)
	MFLAGS += -marm -march=armv7-a
	ASFLAGS += -march=armv7-a
endif

ifeq ($(ARMV7_CORTEX_A9_FLAGS),1)
	MFLAGS += -mtune=cortex-a9
endif

ifeq ($(ARM_FLOAT_ABI_HARD),1)
	MFLAGS += -mfloat-abi=hard
	ASFLAGS += -mfloat-abi=hard
	CFLAGS += -DARM_HARDFP
endif

CORE_DIR := core

DYNAREC_USED = 0

ifeq ($(NO_VERIFY),1)
    CFLAGS       += -DNO_VERIFY
	 CXXFLAGS     += -DNO_VERIFY
	 RZDCY_CFLAGS += -DNO_VERIFY
endif

ifeq ($(NAOMI),1)
    CFLAGS       += -DTARGET_NAOMI
	 CXXFLAGS     += -DTARGET_NAOMI
	 RZDCY_CFLAGS += -DTARGET_NAOMI

    DC_PLATFORM=naomi
	 TARGET_NAME   := reicast_naomi
else
    DC_PLATFORM=dreamcast
endif

ifeq ($(HAVE_GENERIC_JIT),1)
    CFLAGS       += -DTARGET_NO_JIT
	 CXXFLAGS     += -DTARGET_NO_JIT
	 RZDCY_CFLAGS += -DTARGET_NO_JIT
	 CXXFLAGS     += -std=c++11
endif

HOST_CPU_X86=0x20000001
HOST_CPU_ARM=0x20000002
HOST_CPU_MIPS=0x20000003
HOST_CPU_X64=0x20000004

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

ifeq ($(STATIC_LINKING),1)
EXT=a

endif

# Unix
ifneq (,$(findstring unix,$(platform)))
	EXT    ?= so
   TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	LDFLAGS +=  -Wl,--no-undefined
	LIBS += -lrt

	fpic = -fPIC

ifeq ($(WITH_DYNAREC), $(filter $(WITH_DYNAREC), x86_64 x64))
	CFLAGS += -D TARGET_NO_AREC
endif
ifeq ($(WITH_DYNAREC), x86)
	CFLAGS += -D TARGET_NO_AREC
endif

   ifeq ($(FORCE_GLES),1)
		GLES = 1
		GL_LIB := -lGLESv2
	else ifneq (,$(findstring gles,$(platform)))
		GLES = 1
		GL_LIB := -lGLESv2
	else
		GL_LIB := -lGL
	endif

	PLATFORM_EXT := unix

# Raspberry Pi
else ifneq (,$(findstring rpi,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	GLES = 1
	LIBS += -lrt
	GL_LIB := -L/opt/vc/lib -lGLESv2
	INCFLAGS += -I/opt/vc/include
	ifneq (,$(findstring rpi2,$(platform)))
		CPUFLAGS += -DNO_ASM -DARM -D__arm__ -DARM_ASM -D__NEON_OPT -DNOSSE
		CFLAGS = -mcpu=cortex-a7 -mfloat-abi=hard
		CXXFLAGS = -mcpu=cortex-a7 -mfloat-abi=hard
		HAVE_NEON = 0
	else
		CPUFLAGS += -DARMv5_ONLY -DNO_ASM
	endif
	PLATFORM_EXT := unix
	WITH_DYNAREC=arm

# ODROIDs
else ifneq (,$(findstring odroid,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	BOARD := $(shell cat /proc/cpuinfo | grep -i odroid | awk '{print $$3}')
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	GLES = 1
	LIBS += -lrt
	GL_LIB := -lGLESv2
	CPUFLAGS += -DNO_ASM -DARM -D__arm__ -DARM_ASM -D__NEON_OPT -DNOSSE
	CFLAGS += -marm -mfloat-abi=hard -mfpu=neon
	CXXFLAGS += -marm -mfloat-abi=hard -mfpu=neon
	HAVE_NEON = 1
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
	PLATFORM_EXT := unix
	WITH_DYNAREC=arm

# i.MX6
else ifneq (,$(findstring imx6,$(platform)))
	EXT    ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T
	fpic = -fPIC
	GLES = 1
	GL_LIB := -lGLESv2
	LIBS += -lrt
	CPUFLAGS += -DNO_ASM
	PLATFORM_EXT := unix
	WITH_DYNAREC=arm
	HAVE_NEON=1

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

	PLATCFLAGS += -D__MACOSX__ -DOSX
	GL_LIB := -framework OpenGL
	PLATFORM_EXT := unix

	# Target Dynarec
	ifeq ($(ARCH), $(filter $(ARCH), ppc))
		WITH_DYNAREC =
	endif

# iOS
else ifneq (,$(findstring ios,$(platform)))
	EXT    ?= dylib
	ifeq ($(IOSSDK),)
		IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
	endif

	TARGET := $(TARGET_NAME)_libretro_ios.$(EXT)
	DEFINES += -DIOS
	GLES = 1
	WITH_DYNAREC=arm
	PLATFORM_EXT := unix
	HOST_CPU_FLAGS = -DHOST_CPU=$(HOST_CPU_ARM)

	PLATCFLAGS += -DHAVE_POSIX_MEMALIGN -DNO_ASM
	PLATCFLAGS += -DIOS -marm
	CPUFLAGS += -DNO_ASM  -DARM -D__arm__ -DARM_ASM -D__NEON_OPT
	CPUFLAGS += -marm -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp
	SHARED += -dynamiclib
	HAVE_NEON=1

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
	GLES = 1
	WITH_DYNAREC=arm

	PLATCFLAGS += -DHAVE_POSIX_MEMALIGN -DNO_ASM
	PLATCFLAGS += -DIOS -marm
	CPUFLAGS += -DNO_ASM  -DARM -D__arm__ -DARM_ASM -D__NEON_OPT -DNOSSE
	HAVE_NEON=1


# Android
else ifneq (,$(findstring android,$(platform)))
	fpic = -fPIC
	EXT       ?= so
	TARGET := $(TARGET_NAME)_libretro_android.$(EXT)
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common
	GL_LIB := -lGLESv2

	CC = arm-linux-androideabi-gcc
	CXX = arm-linux-androideabi-g++
	WITH_DYNAREC=arm
	GLES = 1
	PLATCFLAGS += -DANDROID
	CPUCFLAGS  += -DNO_ASM
	HAVE_NEON = 1
	CPUFLAGS += -marm -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp -D__arm__ -DARM_ASM -D__NEON_OPT
	CFLAGS += -DANDROID

	PLATFORM_EXT := unix

# QNX
else ifeq ($(platform), qnx)
	fpic = -fPIC
	EXT       ?= so
	TARGET := $(TARGET_NAME)_libretro_$(platform).$(EXT)
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined -Wl,--warn-common
	GL_LIB := -lGLESv2

	CC = qcc -Vgcc_ntoarmv7le
	CC_AS = qcc -Vgcc_ntoarmv7le
	CXX = QCC -Vgcc_ntoarmv7le
	AR = QCC -Vgcc_ntoarmv7le
	WITH_DYNAREC=arm
	GLES = 1
	PLATCFLAGS += -DNO_ASM -D__BLACKBERRY_QNX__
	HAVE_NEON = 1
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
		GLES = 1
		GL_LIB := -lGLESv2
	else
		GL_LIB := -lGL
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
		HAVE_NEON = 1
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
	GLES := 1
	WITH_DYNAREC=
	CPUFLAGS += -Dasm=asmerror -D__asm__=asmerror -DNO_ASM -DNOSSE
	SINGLE_THREAD := 1
	PLATCFLAGS += -Drglgen_resolve_symbols_custom=reicast_rglgen_resolve_symbols_custom \
					  -Drglgen_resolve_symbols=reicast_rglgen_resolve_symbols

NO_REC=0
	HAVE_NEON = 0
	PLATFORM_EXT := unix
	#HAVE_SHARED_CONTEXT := 1

# Windows
else
	EXT       ?= dll
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	SHARED := -shared -shared -static-libgcc -static-libstdc++ -Wl,--version-script=link.T
	LDFLAGS += -lwinmm -lgdi32
	GL_LIB := -lopengl32
	PLATFORM_EXT := win32
	CC = gcc
	CXX = g++
	LDFLAGS += -m32

	CFLAGS += -D TARGET_NO_AREC

endif

ifeq ($(STATIC_LINKING),1)
fpic=
SHARED=
endif

CFLAGS += $(HOST_CPU_FLAGS)
CXXFLAGS += $(HOST_CPU_FLAGS)
RZDCY_CFLAGS += $(HOST_CPU_FLAGS)

include Makefile.common

ifeq ($(DEBUG),1)
OPTFLAGS       := -O0
LDFLAGS  += -g
CFLAGS   += -g
else
OPTFLAGS       := -O2
RZDCY_CFLAGS   += -DNDEBUG
RZDCY_CXXFLAGS += -DNDEBUG
CFLAGS         += -DNDEBUG
CXXFLAGS       += -DNDEBUG
LDFLAGS        += -DNDEBUG
endif
RZDCY_CFLAGS	+= $(CFLAGS) -c $(OPTFLAGS) -DRELEASE -ffast-math -fomit-frame-pointer -D__LIBRETRO__
CFLAGS         += -D__LIBRETRO__

ifeq ($(WITH_DYNAREC), arm)
RZDCY_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mfpu=vfpv3-d16
RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
else
RZDCY_CFLAGS += -DTARGET_LINUX_x86
endif

ifeq ($(NO_THREADS),1)
  RZDCY_CFLAGS += -DTARGET_NO_THREADS
  CFLAGS       += -DTARGET_NO_THREADS
  CXXFLAGS     += -DTARGET_NO_THREADS
else
  LIBS         += -lpthread
endif

ifeq ($(NO_REC),1)
  RZDCY_CFLAGS += -DTARGET_NO_REC
endif

ifeq ($(NO_REND),1)
  RZDCY_CFLAGS += -DNO_REND=1
  CFLAGS 	 += -DNO_REND
  CXXFLAGS += -DNO_REND
endif

ifeq ($(NO_EXCEPTIONS),1)
  RZDCY_CFLAGS += -DTARGET_NO_EXCEPTIONS=1
  CFLAGS 	 += -DTARGET_NO_EXCEPTIONS
  CXXFLAGS += -DTARGET_NO_EXCEPTIONS
endif

ifeq ($(NO_NVMEM),1)
  RZDCY_CFLAGS += -DTARGET_NO_NVMEM=1
  CFLAGS 	 += -DTARGET_NO_NVMEM
  CXXFLAGS += -DTARGET_NO_NVMEM
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fexceptions -fno-rtti -std=gnu++11

CFLAGS   += $(OPTFLAGS) -D RELEASE -c
CFLAGS   += -fno-strict-aliasing -ffast-math
CXXFLAGS += -fno-rtti -fpermissive -fno-operator-names
LIBS     += -lm 

PREFIX        ?= /usr/local

ifeq ($(WITH_DYNAREC), arm)
else
    CC_AS=gcc
    ASFLAGS += -m32
    LDFLAGS += -m32
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

ifeq ($(HAVE_GL), 1)
ifeq ($(GLES),1)
	 RZDCY_CFLAGS += -DHAVE_OPENGLES -DHAVE_OPENGLES2
    CXXFLAGS += -DHAVE_OPENGLES -DHAVE_OPENGLES2
	 CFLAGS   += -DHAVE_OPENGLES -DHAVE_OPENGLES2
else
	 RZDCY_CFLAGS += -DHAVE_OPENGL
    CXXFLAGS += -DHAVE_OPENGL
	 CFLAGS   += -DHAVE_OPENGL
endif
endif

ifeq ($(HAVE_CORE), 1)
	RZDCY_CFLAGS += -DCORE
	CXXFLAGS += -DCORE
	CFLAGS   += -DCORE
endif

CFLAGS     += $(fpic)
CXXFLAGS   += $(fpic)
LDFLAGS    += $(fpic)

OBJECTS := $(SOURCES_CXX:.cpp=.o) $(SOURCES_C:.c=.o) $(SOURCES_ASM:.S=.o)

all: $(TARGET)	
$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(CXX) $(MFLAGS) $(fpic) $(SHARED) $(LDFLAGS) $(OBJECTS) $(LIBS) $(GL_LIB) -o $@
endif

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) $(MFLAGS) $< -o $@

%.o: %.S
	$(CC_AS) $(ASFLAGS) $(INCFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
