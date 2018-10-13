## libosd module 
#
if(NOT RE_CMAKE_CONFIGURED)
error("include config.cmake before libs")
endif()


set(libosd_base_path "${PROJECT_SOURCE_DIR}/core/osd")

include_directories ("${libosd_base_path}")
include_directories ("${libosd_base_path}/rend/khronos")


  #set(libosd_LDEP lgles





## Todo: make option

#../core/rend/gles:		gles.cpp gldraw.cpp gltex.cpp gles.h  
#../core/rend/d3d11:	d3d11.cpp
#../core/rend/soft:		softrend.cpp
#../core/rend/norend:	norend.cpp



if(${HOST_OS} EQUAL ${OS_WINDOWS})
#
  set(libosd_SRCS
	./core/osd/windows/winmain.cpp
	./core/osd/audiobackend/audiostream.cpp
	./core/osd/audiobackend/audiobackend_directsound.cpp
	./core/osd/rend/TexCache.cpp:q
	./core/osd/rend/d3d11/d3d11.cpp
	./core/osd/rend/gles/gles.cpp 
	./core/osd/rend/gles/gltex.cpp
	./core/osd/rend/gles/gldraw.cpp 
	./core/osd/rend/soft/softrend.cpp
#	./core/osd/rend/norend/norend.cpp
  )
#
elseif (${HOST_OS} EQUAL ${OS_LINUX} OR  
		${HOST_OS} EQUAL ${OS_ANDROID})
#
  message("-----------------------------------------------------")
  message("  LINUX OS ")

  add_definitions(-DGLES -DUSE_EVDEV)
  
  link_libraries(pthread dl rt asound Xext GLESv2 EGL)
  
  set(libosd_SRCS
	./core/osd/linux/common.cpp
	./core/osd/linux/context.cpp
	./core/osd/linux/nixprof/nixprof.cpp
	./core/osd/audiobackend/audiostream.cpp
	./core/osd/audiobackend/audiobackend_oss.cpp # add option
	./core/osd/rend/TexCache.cpp
	./core/osd/rend/gles/gles.cpp 
	./core/osd/rend/gles/gltex.cpp
	./core/osd/rend/gles/gldraw.cpp 
  ) # todo: configure linux audio lib options
  
  if(NOT ANDROID)
    list(APPEND libosd_SRCS 
    ./core/osd/linux-dist/x11.cpp
	./core/osd/linux-dist/main.cpp
	./core/osd/linux-dist/evdev.cpp)
	
    add_definitions(-DSUPPORT_X11)  ## don't use GLES ?
    link_libraries(X11)
  else()
    set(libANDROID_SRCS 
      ./shell/android-studio/reicast/src/main/jni/src/Android.cpp
      ./shell/android-studio/reicast/src/main/jni/src/utils.cpp
    # ./shell/android-studio/reicast/src/main/jni/src/XperiaPlay.c
    )
  endif()
  
  if(${HOST_CPU} EQUAL ${CPU_X86} OR ${HOST_CPU} EQUAL ${CPU_X64})
	list(APPEND libosd_SRCS ./core/osd/rend/soft/softrend.cpp)
  endif()
  
#
elseif(${HOST_OS} EQUAL ${OS_DARWIN})
#
error("libosd darwin")
#
elseif(${HOST_OS} EQUAL ${OS_IOS})
#
error("libosd apple ios")
#
elseif(${HOST_OS} EQUAL ${OS_ANDROID})
#
error("libosd android")
#
elseif(${HOST_OS} EQUAL ${OS_NSW_HOS})

  set(libosd_SRCS
	./core/osd/nswitch/main.cpp

	./core/osd/rend/TexCache.cpp
	./core/osd/rend/gles/gles.cpp 
	./core/osd/rend/gles/gltex.cpp
	./core/osd/rend/gles/gldraw.cpp 
  )
else()
#
  error("libosd can't figure out OS use SDL ?")
#
endif()










if(BUILD_LIBS)
  add_library(libosd ${BUILD_LIB_TYPE} ${libosd_SRCS})
else()
  list(APPEND reicast_SRCS ${libosd_SRCS})
endif()

if(NOT ${HOST_OS} EQUAL ${OS_NSW_HOS})
  source_group(TREE ${PROJECT_SOURCE_DIR}/core/osd PREFIX src\\libosd FILES ${libosd_SRCS})
endif()

if(ANDROID)  # *FIXME*
  list(APPEND reicast_SRCS ${libANDROID_SRCS})
endif() 

#[[
	null@Vindras:~/.windev/Projects/rezcast/build$ ls -R ../core/osd/
../core/osd/:
linux  linux-dist  oslib  sdl  windows

../core/osd/linux:
common.cpp  context.cpp  context.h  nixprof  typedefs.h

../core/osd/linux/nixprof:
nixprof.cpp

../core/osd/linux-dist:
dispmanx.cpp  dispmanx.h  evdev.cpp  evdev.h  joystick.cpp  joystick.h  main.cpp  main.h  x11.cpp  x11.h

../core/osd/oslib:
audiobackend_alsa.cpp   audiobackend_coreaudio.cpp    audiobackend_directsound.h  audiobackend_omx.cpp  audiobackend_oss.h           audiostream.cpp  oslib.h
audiobackend_alsa.h     audiobackend_coreaudio.h      audiobackend_libao.cpp      audiobackend_omx.h    audiobackend_pulseaudio.cpp  audiostream.h
audiobackend_android.h  audiobackend_directsound.cpp  audiobackend_libao.h        audiobackend_oss.cpp  audiobackend_pulseaudio.h    logtest

../core/osd/sdl:
sdl.cpp  sdl.h

../core/osd/windows:
winmain.cpp
#]]
