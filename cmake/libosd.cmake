## libosd module 
#
if(NOT RE_CMAKE_CONFIGURED)
error("include config.cmake before libs")
endif()


set(libosd_base_path "${PROJECT_SOURCE_DIR}/core/osd")

include_directories ("${libosd_base_path}")
include_directories ("${libosd_base_path}/rend/khronos")





## Global Sources ##
#
set(libosd_SRCS ./core/osd/oslib.h)


## Todo: make option

#../core/rend/gles:		gles.cpp gldraw.cpp gltex.cpp gles.h  
#../core/rend/d3d11:	d3d11.cpp
#../core/rend/soft:		softrend.cpp
#../core/rend/norend:	norend.cpp

if(NOT NO_GLES)
  list(APPEND libosd_SRCS ./core/osd/rend/TexCache.cpp)

  list(APPEND libosd_SRCS ./core/osd/rend/gles/gles.cpp) 
  list(APPEND libosd_SRCS ./core/osd/rend/gles/gltex.cpp)
  list(APPEND libosd_SRCS ./core/osd/rend/gles/gldraw.cpp)
endif()

if(HEADLESS)
  list(APPEND libosd_SRCS ./core/osd/rend/norend/norend.cpp)
endif()

if(${HOST_CPU} EQUAL ${CPU_X86} OR ${HOST_CPU} EQUAL ${CPU_X64})
  list(APPEND libosd_SRCS ./core/osd/rend/soft/softrend.cpp)
endif()




## Todo: somehow figure audio backend options out, esp for linux
##	finding build packages doesn't help, but option can be for building support

# base stream 
list(APPEND libosd_SRCS ./core/osd/audiobackend/audiostream.cpp)





################ This may make things confusing, can't be helped really  ##############################
#
#	Qt is a cross platform lib,  so it's not tech. OSD... However it replaces many osdeps in the build.
#		Use an option and set def,  and make a bigger mess :|
#



if(USE_QT)	## Ensure CMAKE_PREFIX_PATH is set ##

  find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED) # recent cmake + Qt use CMAKE_PREFIX_PATH to find module in Qt path
  ## Note:  These are only for finding, if you don't include in target_link_libraries you won't even get include paths updated

  set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON)
  
  set(d_qt ./core/osd/qt)

  list(APPEND libosd_SRCS	# qt5_wrap_cpp(${d_qt}/ )
    ${d_qt}/mainwindow.h ${d_qt}/mainwindow.cpp		# should use qt5_wrap_cpp() functions so the rest of the code won't get moc called on it from CMAKE_AUTOMOC
  )
  
  add_definitions(-DUSE_QT -DUI_QT -DOSD_QT)

  

#elseif (USE_SDL)
###################################################################################################





elseif (${HOST_OS} EQUAL ${OS_WINDOWS} AND NOT USE_QT)
#
  list(APPEND libosd_SRCS 
	./core/osd/windows/winmain.cpp
	./core/osd/rend/d3d11/d3d11.cpp
	./core/osd/audiobackend/audiobackend_directsound.cpp
  )
#
elseif (${HOST_OS} EQUAL ${OS_LINUX} OR  
		${HOST_OS} EQUAL ${OS_ANDROID})
#
  message("-----------------------------------------------------")
  message("  LINUX OS ")

  add_definitions(-DGLES -DUSE_EVDEV)
  
  link_libraries(pthread dl rt asound Xext GLESv2 EGL)
  
  list(APPEND libosd_SRCS 
	./core/osd/linux/common.cpp
	./core/osd/linux/context.cpp
	./core/osd/linux/nixprof/nixprof.cpp

	./core/osd/audiobackend/audiobackend_oss.cpp # add option
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
#
  error("libosd NSW HOS")
  list(APPEND libosd_SRCS ./core/osd/nswitch/main.cpp)
#
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
