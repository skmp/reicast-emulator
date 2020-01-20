#LOCAL_PATH:=

#MFLAGS	:= -marm -march=armv7-a -mtune=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=softfp
#ASFLAGS	:= -march=armv7-a -mfpu=vfp-d16 -mfloat-abi=softfp
#LDFLAGS	:= -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common

RZDCY_SRC_DIR ?= $(call my-dir)
VERSION_HEADER := $(RZDCY_SRC_DIR)/version.h

RZDCY_MODULES	:=	cfg/ hw/arm7/ hw/aica/ hw/holly/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/interpr/ hw/sh4/modules/ plugins/ profiler/ oslib/ \
 hw/extdev/ hw/arm/ hw/naomi/ imgread/ ./ deps/coreio/ deps/zlib/ deps/chdr/ deps/crypto/ \
 deps/libelf/ deps/cdipsr/ arm_emitter/ rend/ reios/ deps/libpng/ deps/xbrz/ \
 deps/xxhash/ deps/libzip/ deps/imgui/ archive/ input/ utils/ utils/glwrap/ gui/

ifdef SCRIPTING
	RZDCY_MODULES += scripting/
	RZDCY_MODULES += deps/lua/
endif

ifdef _NO_WEBUI
	RZDCY_MODULES += webui/
	RZDCY_MODULES += deps/libwebsocket/

	ifdef FOR_ANDROID
		RZDCY_MODULES += deps/ifaddrs/
	endif
endif


ifdef CPP_REC
    RZDCY_MODULES += jit/backend/cpp/
endif

ifdef X86_REC
    RZDCY_MODULES += jit/backend/x86/ jit/emitter/x86/
endif

ifdef X64_REC
    RZDCY_MODULES += jit/backend/x64/
endif

ifdef ARM32_REC
    RZDCY_MODULES += jit/backend/arm32/ jit/emitter/arm/ deps/vixl/ deps/vixl/aarch32/
endif

ifdef ARM64_REC
    RZDCY_MODULES += jit/backend/arm64/ deps/vixl/ deps/vixl/aarch64/
endif

# rend options
ifndef NO_REND_GLES
    RZDCY_MODULES += rend/gles/
endif

ifndef NO_REND_GL4
	RZDCY_MODULES += rend/gl4/
endif

ifndef NO_REND_NONE
	RZDCY_MODULES += rend/norend/
endif

ifdef HAS_SOFTREND
	RZDCY_CFLAGS += -DTARGET_SOFTREND
	RZDCY_MODULES += rend/soft/
endif

# glinit options
ifdef SUPPORT_EGL
	RZDCY_CFLAGS  += -D SUPPORT_EGL
	RZDCY_MODULES += utils/glinit/egl/
endif

ifdef SUPPORT_GLX
	RZDCY_CFLAGS  += -D SUPPORT_GLX
	RZDCY_MODULES += utils/glinit/glx/
endif

ifdef SUPPORT_WGL
	RZDCY_CFLAGS  += -D SUPPORT_WGL
	RZDCY_MODULES += utils/glinit/wgl/
endif
    
ifdef SUPPORT_SDL
	RZDCY_CFLAGS  += -D SUPPORT_SDL
	RZDCY_MODULES += utils/glinit/sdl/
endif

ifndef NO_NIXPROF
    RZDCY_MODULES += linux/nixprof/
endif

# platform selection
ifdef PLATFORM_ANDROID
    RZDCY_MODULES += android/ deps/libandroid/ linux/ oslib/posix/
endif

ifdef PLATFORM_SDL
    RZDCY_MODULES += sdl/
endif

ifdef PLATFORM_LINUX
    RZDCY_MODULES += linux-dist/ linux/ oslib/posix/
endif

ifdef PLATFORM_WINDOWS
    RZDCY_MODULES += windows/ oslib/windows/
endif

RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/rend/gles -I$(RZDCY_SRC_DIR)/deps \
		 -I$(RZDCY_SRC_DIR)/deps/vixl -I$(RZDCY_SRC_DIR)/khronos

ifdef USE_MODEM
	RZDCY_CFLAGS += -DENABLE_MODEM -I$(RZDCY_SRC_DIR)/deps/picotcp/include -I$(RZDCY_SRC_DIR)/deps/picotcp/modules
	RZDCY_MODULES += hw/modem/ deps/picotcp/modules/ deps/picotcp/stack/
endif

ifdef NO_REC
	RZDCY_CFLAGS += -DTARGET_NO_REC
else
	RZDCY_MODULES += hw/sh4/dyna/
endif

ifdef CHD5_FLAC
	RZDCY_CFLAGS += -DCHD5_FLAC -I$(RZDCY_SRC_DIR)/deps/flac/src/libFLAC/include/ -I$(RZDCY_SRC_DIR)/deps/flac/include
	RZDCY_CFLAGS += -DPACKAGE_VERSION=\"1.3.2\" -DFLAC__HAS_OGG=0 -DFLAC__NO_DLL -DHAVE_LROUND -DHAVE_STDINT_H -DHAVE_STDLIB_H -DHAVE_SYS_PARAM_H
	RZDCY_MODULES += deps/flac/src/libFLAC/
endif

# 7-Zip/LZMA settings (CHDv5)
ifdef CHD5_LZMA
	RZDCY_MODULES += deps/lzma/
	RZDCY_CFLAGS += -D_7ZIP_ST -DCHD5_LZMA
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11

RZDCY_FILES := $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cpp))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cc))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.c))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.S))

$(VERSION_HEADER):
	echo "#define REICAST_VERSION \"`git describe --tags --always`\"" > $(VERSION_HEADER)
	echo "#define GIT_HASH \"`git rev-parse --short HEAD`\"" >> $(VERSION_HEADER)
	echo "#define BUILD_DATE \"`date '+%Y-%m-%d %H:%M:%S %Z'`\"" >> $(VERSION_HEADER)

