## libosd module 
#
if(NOT RE_CMAKE_CONFIGURED)
error("include config.cmake before libs")
endif()


set(libosd_base_path "${PROJECT_SOURCE_DIR}/core/osd")

include_directories ("${libosd_base_path}")
include_directories ("${libosd_base_path}/rend/khronos")


set(d_osd  ./core/osd)		# These help keep paths somewhat configurable during dev.
set(d_rend ./core/osd/rend)

## Global Sources ##
#
set(libosd_SRCS ${d_osd}/oslib.h)





### Rend ###
#

# texture cache
list(APPEND libosd_SRCS ${d_rend}/TexCache.cpp)


## Todo: make option
#
#../core/rend/gles:		gles.cpp gldraw.cpp gltex.cpp gles.h  
#../core/rend/d3d11:	d3d11.cpp
#../core/rend/soft:		softrend.cpp
#../core/rend/norend:	norend.cpp

if(NOT NO_GLES)
  list(APPEND libosd_SRCS ${d_rend}/gles/gles.cpp) 
  list(APPEND libosd_SRCS ${d_rend}/gles/gltex.cpp)
  list(APPEND libosd_SRCS ${d_rend}/gles/gldraw.cpp)
endif()

if(HEADLESS)
  list(APPEND libosd_SRCS ${d_rend}/norend/norend.cpp)
endif()

if(${HOST_CPU} EQUAL ${CPU_X86} OR ${HOST_CPU} EQUAL ${CPU_X64})
  list(APPEND libosd_SRCS ${d_rend}/soft/softrend.cpp)
endif()





## Todo: somehow figure audio backend options out, esp for linux
##	finding build packages doesn't help, but option can be for building support

# base stream 
list(APPEND libosd_SRCS ${d_osd}/audiobackend/audiostream.cpp)





################ This may make things confusing, can't be helped really  ##############################
#
#	Qt is a cross platform lib,  so it's not tech. OSD... However it replaces many osdeps in the build.
#		Use an option and set def,  and make a bigger mess :|
#


if(USE_QT)	## Ensure CMAKE_PREFIX_PATH is set ##
#
  find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED) # recent cmake + Qt use CMAKE_PREFIX_PATH to find module in Qt path
  ## Note:  These are only for finding, if you don't include in target_link_libraries you won't even get include paths updated

  set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON)

  list(APPEND libosd_SRCS	# qt5_wrap_cpp(${d_qt}/ )		# should use qt5_wrap_cpp() functions so the rest of the code won't get moc called on it from CMAKE_AUTOMOC
	  ${d_osd}/qt/qt_ui.h      ${d_osd}/qt/qt_ui.cpp
    ${d_osd}/qt/qt_osd.h     ${d_osd}/qt/qt_osd.cpp
  )
  
  add_definitions(-DUSE_QT -DQT_UI -DQT_OSD)
 



#
elseif(USE_SDL)
#
  find_package(SDL)

  list(APPEND libosd_SRCS ${d_osd}/sdl/sdl_ui.cpp)
  
  add_definitions(-DUSE_SDL -DSDL_UI)

endif()


## WIP ./osd/$OS$/$OS$_{osd,ui,rend?}
#

if (${HOST_OS} EQUAL ${OS_WINDOWS} AND NOT TARGET_UWP)
#
  message("-----------------------------------------------------")
  message(" OS: Windows ")

  list(APPEND libosd_SRCS ${d_osd}/windows/win_osd.cpp)
  
  list(APPEND libosd_SRCS ${d_osd}/audiobackend/audiobackend_directsound.cpp)

  if(NOT USE_QT AND NOT USE_SDL AND NOT TARGET_UWP)
    list(APPEND libosd_SRCS 
      ${d_osd}/windows/winmain.cpp	#win_ui
      ${d_osd}/rend/d3d11/d3d11.cpp
    )
  endif()
#
elseif(TARGET_UWP)
#
  message(" Subtarget: UWP / Windows Store ")

  list(APPEND libosd_SRCS ${d_osd}/uwp/reicastApp.cpp)
  list(APPEND libosd_SRCS ${d_osd}/windows/win_osd.cpp)

  list(APPEND libosd_SRCS ${d_osd}/audiobackend/audiobackend_directsound.cpp)

	# angle ? is setup elsewhere
#
elseif (${HOST_OS} EQUAL ${OS_LINUX}   OR  ## This should prob be if POSIX_COMPAT : ./osd/posix_base_osd
	    ${HOST_OS} EQUAL ${OS_ANDROID} )
#
  message("-----------------------------------------------------")
  message(" OS: Linux ")

#  list(APPEND libosd_SRCS ./core/osd/linux/lin_osd.cpp)
  
  if(NOT USE_QT AND NOT USE_SDL)

    list(APPEND libosd_SRCS 
      ${d_osd}/linux/common.cpp
      ${d_osd}/linux/context.cpp
      ${d_osd}/linux/nixprof/nixprof.cpp

      ${d_osd}/audiobackend/audiobackend_oss.cpp # add option
    ) # todo: configure linux audio lib options
  
    if(NOT ANDROID)
      list(APPEND libosd_SRCS 
		${d_osd}/linux-dist/x11.cpp
		${d_osd}/linux-dist/main.cpp
		${d_osd}/linux-dist/evdev.cpp)
	
      add_definitions(-DSUPPORT_X11)  ## don't use GLES ?
      link_libraries(X11)
    else()
      set(libANDROID_SRCS 
        ./shell/android-studio/reicast/src/main/jni/src/Android.cpp
        ./shell/android-studio/reicast/src/main/jni/src/utils.cpp
    #   ./shell/android-studio/reicast/src/main/jni/src/XperiaPlay.c
      )
    endif() # ANDROID

  endif() # NOT USE_QT
  

  add_definitions(-DGLES -DUSE_EVDEV)
  
  link_libraries(pthread dl rt asound Xext GLESv2 EGL)
  
#elseif(${HOST_OS} EQUAL ${OS_ANDROID})

elseif(${HOST_OS} EQUAL ${OS_DARWIN})
#
  message("-----------------------------------------------------")
  message(" OS: Apple OS X ")


  list(APPEND objc_SRCS
    ./shell/apple/emulator-osx/emulator-osx/osx-main.mm
    ./shell/apple/emulator-osx/emulator-osx/AppDelegate.swift
    ./shell/apple/emulator-osx/emulator-osx/EmuGLView.swift
  )

  set_source_files_properties(${objc_SRCS} PROPERTIES COMPILE_FLAGS "-x objective-c++")



  list(APPEND libosd_SRCS ${objc_SRCS}
    ${d_osd}/linux/common.cpp
    ${d_osd}/linux/context.cpp
    ${d_osd}/audiobackend/audiobackend_coreaudio.cpp
    # if NOT USE_SWIFT / ObjC
    #${d_osd}/apple/osx_osd.cpp
    )



elseif(${HOST_OS} EQUAL ${OS_IOS})
#
  message("-----------------------------------------------------")
  message(" OS: Apple iOS ")

  list(APPEND libosd_SRCS ${d_osd}/apple/ios_osd.cpp)
  #if NOT QT OR SDL then add NATIVE UI code 
#
elseif(${HOST_OS} EQUAL ${OS_NSW_HOS})
#
  message("-----------------------------------------------------")
  message(" OS: Switch HOS ")
  
  list(APPEND libosd_SRCS ${d_osd}/switch/nsw_osd.cpp)
#
elseif(${HOST_OS} EQUAL ${OS_PS4_BSD})
#
  message("-----------------------------------------------------")
  message(" OS: Playstation4 BSD ")
  ## This will prob use posix / linux/common.cpp as a base too *FIXME* 
  list(APPEND libosd_SRCS
	${d_osd}/ps4/ps4_util.S
	${d_osd}/ps4/ps4_osd.cpp
	
    ${d_osd}/linux/common.cpp

  # ${d_osd}/audiobackend/audiobackend_oss.cpp
  )


else()
#
  message("libosd can't figure out OS use SDL ?")
  error()
#
endif()








if(BUILD_LIBS)
  add_library(libosd ${BUILD_LIB_TYPE} ${libosd_SRCS})
else()
  list(APPEND reicast_SRCS ${libosd_SRCS})
endif()

if(NOT ${HOST_OS} EQUAL ${OS_NSW_HOS} AND NOT TARGET_OSX AND NOT TARGET_IOS)
  source_group(TREE ${libosd_base_path} PREFIX src\\libosd FILES ${libosd_SRCS})
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
