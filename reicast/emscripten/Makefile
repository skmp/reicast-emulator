
LOCAL_PATH := $(call my-dir)
PLATFORM_LINUX := 1
SUPPORT_EGL := 1
CPP_REC := 1
NO_NIXPROF := 1
RZDCY_SRC_DIR = ../../libswirl

include $(RZDCY_SRC_DIR)/core.mk

CC_PREFIX ?= 

CXX=${CC_PREFIX}em++
CC=${CC_PREFIX}emcc
#AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}emstrip

LD=${CC}

MFLAGS := 
ASFLAGS := 

LDFLAGS	:= 


CXXONLYFLAGS := -std=c++14
CXXFLAGS := -s USE_PTHREADS=1 -s USE_SDL=2 -O3 -D RELEASE -c -D TARGET_EMSCRIPTEN -D TARGET_NO_JIT -D TARGET_NO_EXCEPTIONS -D TARGET_NO_WEBUI -D TARGET_NO_COREIO_HTTP -D TARGET_NO_OPENMP -D SUPPORT_EGL -D FEAT_HAS_NIXPROF=0 
CXXFLAGS += -D USE_SDL_AUDIO=1
CXXFLAGS += #-g2 # for debug
CXXFLAGS += -fno-strict-aliasing
CXXFLAGS += -ffast-math

CXXFLAGS += $(CFLAGS) $(MFLAGS) -fexceptions -frtti


INCS	:= -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include

LIBS	:= #emscripten has all the basic libs, and egl, and sdl, built in

OBJECTS=$(RZDCY_FILES:.cpp=.build.obj)
OBJECTS:=$(OBJECTS:.c=.build.obj)
OBJECTS:=$(OBJECTS:.S=.build.obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


EXECUTABLE=reicast.html

all: $(CPPFILES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	echo $(RZDCY_FILES)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS)  $(OBJECTS) $(LIBS) -o $@ --preload-file data/dc_boot.bin --preload-file data/dc_flash.bin  --preload-file roms/ct.chd --preload-file emu.cfg -s TOTAL_MEMORY=536870912 -O3 -s USE_PTHREADS=1 -s USE_SDL=2 #-g2 #for debug
obj/%.build.obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $(CXXONLYFLAGS) $< -o $@
	
obj/%.build.obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $< -o $@

obj/%.build.obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $< -o $@


clean:
	rm $(OBJECTS) $(EXECUTABLE) -f

