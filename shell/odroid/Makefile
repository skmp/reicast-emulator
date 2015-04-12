LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
WEBUI :=1
USE_OSS :=1
USE_VMEM_FILE :=1
#NO_REND := 1
#NO_X11 := 1
RZDCY_SRC_DIR = ../../core

include $(RZDCY_SRC_DIR)/core.mk

#CC_PREFIX ?= arm-none-linux-gnueabi-

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip

LD=${CC}

MFLAGS := -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=hard

LDFLAGS	:=	-g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 

CXXFLAGS := -g -O3  -D RELEASE -c -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7
CXXFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CXXFLAGS += -ffast-math -ftree-vectorize -fprefetch-loop-arrays 
CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-exceptions -fno-rtti
CXXFLAGS += -DARM_HARDFP -DGLES -DODROID -DUSES_HOMEDIR
#-std=c++0x

INCS	:= -I/usr/include -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include

LIBS    := -L/usr/lib/arm-linux-gnueabihf/ -L../linux-deps/lib
LIBS    += -lm -lrt -lEGL -lGLESv2 #-lglslcompiler -lIMGegl -lpvr2d -lsrv_um 
LIBS    += -lpthread

ifndef NO_X11
	CXXFLAGS += -DSUPPORT_X11
endif

ifdef PGO_MAKE
	CXXFLAGS += -fprofile-generate -pg
	LDFLAGS	+= -fprofile-generate
else
	CXXFLAGS += -fomit-frame-pointer
endif

ifdef PGO_USE
	CXXFLAGS += -fprofile-use
endif

ifdef LTO_TEST
	CXXFLAGS += -flto -fwhole-program 
	LDFLAGS +=-flto -fwhole-program 
endif

ifdef USE_OSS
        CXXFLAGS += -DUSE_OSS
else
        LIBS += -lasound
endif

ifdef USE_VMEM_FILE
        CXXFLAGS += -DUSE_VMEM_FILE
endif

ifndef NO_X11
	LIBS += -lX11
endif

OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE=reicast.elf

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
	echo $(RZDCY_FILES)

$(EXECUTABLE): $(OBJECTS)
	echo $(RZDCY_FILES)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS)  $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	  cp $< $@ && $(STRIP) $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $< -o $@
	
obj/%.build_obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $< -o $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $< -o $@


clean:
	rm $(OBJECTS) $(EXECUTABLE) -f
install:
	cp -av reicast.elf /usr/local/bin/reicast
	cp -av reicast_start /usr/local/bin/
	cp -av reicast.desktop /usr/local/share/applications/
	cp -av reicast.png /usr/local/share/pixmaps
