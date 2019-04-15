cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(TNAME reicast)

project(${TNAME})

enable_language(ASM)


set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)


## built-in cmake modules #
#
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckCSourceCompiles)



set(CMAKE_BINARY_DIR       ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR})



set(reicast_root_path "${CMAKE_CURRENT_SOURCE_DIR}")
set(reicast_core_path "${CMAKE_CURRENT_SOURCE_DIR}/core")

include_directories ("${reicast_core_path}")




list(APPEND CMAKE_MODULE_PATH "${reicast_root_path}/cmake")


include(GetGitRevisionDescription)

git_describe(GIT_VERSION --tags)

configure_file(${reicast_core_path}/version.h.in ${reicast_core_path}/version.h @ONLY)





## reicast build modules #
#

set(reicast_SRCS "")

include(config)			# configure build settings, must be first
include(libdreamcast)	# main emulator core lib
include(deps)
include(libosd)			# OS Dep. base 



## reicast_SRCS   *should* only be entry point and bare necessities
#
list(APPEND reicast_SRCS
#
  ./core/stdclass.cpp
#
  ./core/cfg/cl.cpp
  ./core/cfg/cfg.cpp
  ./core/cfg/ini.cpp
#
  ./core/imgread/ImgReader.cpp
  ./core/imgread/common.cpp
  ./core/imgread/ioctl.cpp
  ./core/imgread/gdi.cpp
  ./core/imgread/cdi.cpp
  ./core/imgread/chd.cpp
#
)  # todo add all the deps or add them to lib

if(TARGET_PS4)
  set(binSuffix ".elf")
endif()

if (${HOST_OS} EQUAL ${OS_UWP})
  # Temporarily only building the UWP app because some stuff won't build right now
  message(" FIXME/TODO: This builds ONLY the UWP frontend!")
  
  # Don't build anything else
  set(reicast_SRCS "")
  set(deps_SRCS "")
  
  # build UWP app that only creates a window and does nothing else
  list(APPEND deps_SRCS ${reicast_core_path}/osd/uwp/reicastApp.cpp)
  list(APPEND deps_SRCS ${reicast_core_path}/osd/uwp/reicastApp.h)
endif()

add_executable(${TNAME}${binSuffix} ${reicast_SRCS} ${deps_SRCS})

if (${HOST_OS} EQUAL ${OS_UWP})
  set_property(TARGET ${TNAME} PROPERTY VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${MIN_PLATFORM_VERSION}")
  # Shouldn't actually need the below but just trying things
  #set_property(TARGET ${TNAME} PROPERTY VS_SDK_REFERENCES "Micrsoft.Universal.CRT.Debug, Version=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()

if(NOT ${HOST_OS} EQUAL ${OS_NSW_HOS})
  source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX src FILES ${reicast_SRCS}) #${PROJECT_SOURCE_DIR}/core
endif()


if(USE_QT)
  target_link_libraries(${TNAME} Qt5::Widgets Qt5::OpenGL) # libdreamcast libosd) #  libui , externals
endif()



if(APPLE)
  enable_language(Swift)
  set_property(TARGET ${TNAME} PROPERTY XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "./shell/apple/emulator-osx/emulator-osx/emulator-osx-Bridging-Header.h")

target_link_libraries(${TNAME}
#  "-framework Cocoa"
#  "-framework AppKit"
  "-framework CoreData"
  "-framework CoreAudio"
  "-framework AudioUnit"
  "-framework AudioToolbox"
  "-framework Foundation"
)

#### OSX Notes,  when not using xcode you have to make app bundle,  edit plist and copy,  convert MainMenu.xib to nib and copy, 
#null@devpc:~$ /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/MacOS/reicast ; exit;
#2019-03-18 14:28:44.842 reicast[11468:131797] Unknown class _TtC12emulator_osx9EmuGLView in Interface Builder file at path /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/Resources/MainMenu.nib.
#2019-03-18 14:28:44.842 reicast[11468:131797] Unknown class _TtC12emulator_osx11AppDelegate in Interface Builder file at path /Users/null/Documents/projects/reicast-emulator/bin/RelWithDebInfo/Reicast.app/Contents/Resources/MainMenu.nib.
#2019-03-18 14:28:44.860 reicast[11468:131797] Failed to connect (window) outlet from (NSObject) to (NSWindow): missing setter or instance variable
#



endif()


if(DEBUG_CMAKE)
  message(" ------------------------------------------------")
  message(" - HOST_OS: ${HOST_OS} - HOST_CPU: ${HOST_CPU}   ")
  message(" - host_os: ${host_os} - host_arch: ${host_arch} ")
  message(" ------------------------------------------------")
  message("  C  Flags: ${CMAKE_C_FLAGS} ")
  message(" CXX Flags: ${CMAKE_CXX_FLAGS} ")
  message(" LINK_DIRS: ${LINK_DIRECTORIES}")
  message("LINK_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  message(" ------------------------------------------------\n")
endif()
