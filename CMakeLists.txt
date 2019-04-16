cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(TNAME reicast)

project(${TNAME})

enable_language(ASM)

set(DEBUG_CMAKE On)
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



set(reicast_root_path  "${CMAKE_CURRENT_SOURCE_DIR}")
set(reicast_core_path  "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(reicast_shell_path "${CMAKE_CURRENT_SOURCE_DIR}/shell")


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

add_executable(${TNAME}${binSuffix} ${reicast_SRCS} ${deps_SRCS}) 


if(NOT ${HOST_OS} EQUAL ${OS_NSW_HOS} AND NOT TARGET_UWP)
  source_group(TREE ${PROJECT_SOURCE_DIR} PREFIX src FILES ${reicast_SRCS}) #${PROJECT_SOURCE_DIR}/core
endif()


if(USE_QT)
  target_link_libraries(${TNAME} Qt5::Widgets Qt5::OpenGL) # libdreamcast libosd) #  libui , externals
endif()


if(TARGET_UWP)
	set(uwp_path   "${reicast_shell_path}/uwp")
	set(angle_path "${reicast_shell_path}/angle")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:${angle_path}/lib")
	
	set_target_properties(${TNAME} PROPERTIES VS_WINRT_EXTENSIONS TRUE)
	set_target_properties(${TNAME} PROPERTIES VS_WINRT_COMPONENT FALSE)

	
	set(appx_src_path "${uwp_path}/appx")

	set(ASSET_FILES
	    ${appx_src_path}/Logo.scale-100.png
	    ${appx_src_path}/SmallLogo.scale-100.png
	    ${appx_src_path}/SplashScreen.scale-100.png
	    ${appx_src_path}/StoreLogo.scale-100.png
	    )

	set(CONTENT_FILES ${appx_src_path}/Package.appxmanifest)

	set(RESOURCE_FILES ${CONTENT_FILES} ${ASSET_FILES} ${appx_src_path}/VSXaml_TemporaryKey.pfx)
	

	set_property(SOURCE ${CONTENT_FILES} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ASSET_FILES}   PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ASSET_FILES}   PROPERTY VS_DEPLOYMENT_LOCATION "${appx_src_path}")

#	set_target_properties(${TNAME} PROPERTIES VS_DESKTOP_EXTENSIONS_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
	#include_directories(${angle_path}/include)		# Not needed ? we have gles headers
	
	#target_link_libraries(${TNAME} "${angle_lib_path}/libGLESv2.lib" "${angle_lib_path}/libEGL.lib")  #should be done already
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
  message(" - BUILD_COMPILER: ${BUILD_COMPILER}             ")
  message(" ------------------------------------------------")
  message(" - CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
  message(" ------------------------------------------------")
  message(" - FEAT_SHREC  ${FEAT_SHREC}                     ")
  message(" - FEAT_AREC   ${FEAT_AREC}                      ")
  message(" - FEAT_DSPREC ${FEAT_DSPREC}                    ")
  message(" ------------------------------------------------")
  message("  C  Flags: ${CMAKE_C_FLAGS} ")
  message(" CXX Flags: ${CMAKE_CXX_FLAGS} ")
  message(" LINK_DIRS: ${LINK_DIRECTORIES}")
  message("LINK_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  message(" ------------------------------------------------\n")
endif()





