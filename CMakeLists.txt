cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

project(reicast)

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
include(deps)
include(libosd)			# OS Dep. base 
include(libdreamcast)	# main emulator core lib



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

add_executable(reicast${binSuffix} ${reicast_SRCS} ${deps_SRCS}) 

if(NOT ${HOST_OS} EQUAL ${OS_NSW_HOS})
  source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX src FILES ${reicast_SRCS}) #${PROJECT_SOURCE_DIR}/core
endif()


if(USE_QT)
  target_link_libraries(reicast Qt5::Widgets Qt5::OpenGL) # libdreamcast libosd) #  libui , externals
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
