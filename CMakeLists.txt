cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

set(TNAME reicast)

project(${TNAME})

enable_language(ASM)
enable_language(ASM_MASM)

set(DEBUG_CMAKE ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(reicast_root_path  "${CMAKE_CURRENT_SOURCE_DIR}")
set(reicast_core_path  "${reicast_root_path}/libswirl")
set(reicast_shell_path "${reicast_root_path}/reicast")

message("reicast_root_path: ${reicast_root_path}")
message("reicast_core_path: ${reicast_core_path}")
message("reicast_shell_path: ${reicast_shell_path}")

list(APPEND CMAKE_MODULE_PATH "${reicast_shell_path}/cmake")

include(GetGitRevisionDescription)
git_describe(GIT_VERSION --tags)
configure_file(${reicast_core_path}/version.h.in ${reicast_core_path}/version.h @ONLY)

## reicast build modules #
#

set(reicast_SRCS "")

include(config)			# configure build settings, must be first

# FIXME: some better way?
add_definitions(/DSCRIPTING)

### libdreamcast.cmake #########################################################################

set(d_core ${reicast_core_path})

message(${d_core})

file(GLOB_RECURSE hw_SRCS	${d_core}/hw/*.cpp			${d_core}/hw/*.h)
file(GLOB_RECURSE lxdream_SRCS	${d_core}/gpl/lxdream/*.cpp			${d_core}/gpl/lxdream/*.h)

file(GLOB cfg_SRCS			${d_core}/cfg/*.cpp			${d_core}/cfg/*.h)
file(GLOB rend_SRCS			${d_core}/rend/*.cpp		${d_core}/rend/*.h)
file(GLOB input_SRCS		${d_core}/input/*.cpp		${d_core}/input/*.h)
file(GLOB reios_SRCS		${d_core}/reios/*.cpp		${d_core}/reios/*.h)
file(GLOB imgread_SRCS		${d_core}/imgread/*.cpp		${d_core}/imgread/*.h)
file(GLOB profiler_SRCS		${d_core}/profiler/*.cpp	${d_core}/profiler/*.h)
file(GLOB archive_SRCS		${d_core}/archive/*.cpp		${d_core}/archive/*.h)
file(GLOB glwrap_SRCS		${d_core}/utils/glwrap/*.cpp )
file(GLOB scripting_SRCS	${d_core}/scripting/*.cpp	${d_core}/scripting/*.h)
file(GLOB utils_SRCS	${d_core}/utils/*.cpp	${d_core}/utils/*.h)
file(GLOB gui_SRCS  ${d_core}/gui/*.cpp ${d_core}/gui/*.h)

if(${HOST_OS} EQUAL ${OS_WINDOWS})
  file(GLOB wgl_SRCS  ${d_core}/utils/glinit/wgl/*.cpp )
endif()

#### option(rend)
if(NOT ${HOST_OS} EQUAL ${OS_DARWIN})
  file(GLOB gl4_SRCS			${d_core}/rend/gl4/*.cpp	${d_core}/rend/gl4/*.h)
endif()
file(GLOB gles_SRCS				${d_core}/rend/gles/*.cpp	${d_core}/rend/gles/*.h)
file(GLOB norend_SRCS			${d_core}/rend/norend/*.cpp	${d_core}/rend/norend/*.h)
file(GLOB softrend_SRCS		${d_core}/rend/soft/*.cpp	${d_core}/rend/soft/*.h)
if(${HOST_OS} EQUAL ${OS_WINDOWS})
  file(GLOB d3d11_SRCS		${d_core}/rend/d3d11/*.cpp	${d_core}/rend/d3d11/*.h)
endif()

set(core_SRCS
  ${lxdream_SRCS}
  ${hw_SRCS}
  ${cfg_SRCS}
  ${rend_SRCS}
  ${glwrap_SRCS}
  ${gl4_SRCS}
  ${gles_SRCS}
  ${norend_SRCS}
  ${softrend_SRCS}
  ${d3d11_SRCS}
  ${input_SRCS}
  ${reios_SRCS}
  ${imgread_SRCS}
  ${profiler_SRCS}
  ${scripting_SRCS}
  ${utils_SRCS}
  ${gui_SRCS}
  ${egl_SRCS}
  ${wgl_SRCS}
  ${d_core}/archive/archive.cpp ${d_core}/archive/archive.h
  ${d_core}/libswirl.cpp
  ${d_core}/stdclass.cpp
  ${d_core}/dispframe.cpp
  ${d_core}/serialize.cpp
)

if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC} OR ${BUILD_COMPILER} EQUAL ${COMPILER_CLANG})
  list(APPEND core_SRCS ${archive_SRCS})
endif()

if(${FEAT_SHREC} EQUAL ${DYNAREC_JIT}) 

  if(${HOST_CPU} EQUAL ${CPU_X86})
    list(APPEND core_SRCS 
      ${d_core}/jit/backend/x86/rec_x86_driver.cpp
      ${d_core}/jit/backend/x86/rec_x86_asm.cpp	# change for linux , rec_lin86_asm.S
	  ${d_core}/jit/backend/x86/rec_x86_ngen.h

      ${d_core}/jit/emitter/x86/x86_emitter.cpp
     )
  elseif(${HOST_CPU} EQUAL ${CPU_ARM})
    list(APPEND core_SRCS 
      ${d_core}/rec-ARM/ngen_arm.S
      ${d_core}/rec-ARM/rec_arm.cpp
	)
  elseif(${HOST_CPU} EQUAL ${CPU_X64})

    list(APPEND core_SRCS ${d_core}/jit/backend/x64/rec_x64.cpp ${d_core}/jit/backend/x64/x64_regalloc.h)

  elseif(${HOST_CPU} EQUAL ${CPU_A64})
    list(APPEND core_SRCS ${d_core}/rec-ARM64/rec_arm64.cpp ${d_core}/rec-ARM64/arm64_regalloc.h)

  else()
    message(" FEAT_SHREC==DYNAREC_JIT && HOST_CPU Unknown Default add arch or disable rec if not avail.")
	error()
  endif()

elseif(${FEAT_SHREC} EQUAL ${DYNAREC_CPP})
    list(APPEND core_SRCS ${d_core}/jit/backend/cpp/rec_cpp.cpp)
endif()

add_definitions(/DFEAT_HAS_SOFTREND=1)

if(${HOST_OS} EQUAL ${OS_LINUX})
  add_definitions(-DLUA_USE_LINUX)
endif()

### deps.cmake #################################################################################

set(d_deps ${reicast_core_path}/deps)
set(gpl_deps ${reicast_core_path}/gpl/deps)

include_directories ("${d_deps}")
include_directories ("${gpl_deps}/picotcp/include")
include_directories ("${gpl_deps}/picotcp/modules")

file(GLOB xbyak_H	${d_deps}/xbyak/*.h)		# include headers into cmake target/project view 

file(GLOB chdr_SRCS  ${d_deps}/chdr/*.c)
if(${HOST_OS} EQUAL ${OS_WINDOWS})
  file(GLOB dirent_SRCS  ${d_deps}/dirent/*.c)
endif()
file(GLOB lzma_SRCS  ${d_deps}/lzma/*.c)
file(GLOB lz_SRCS    ${d_deps}/zlib/*.c)
file(GLOB lzip_SRCS  ${d_deps}/libzip/*.c)
file(GLOB lpng_SRCS  ${d_deps}/libpng/*.c)
file(GLOB lelf_SRCS  ${d_deps}/libelf/el*.cpp)
file(GLOB crypt_SRCS ${d_deps}/crypto/*.cpp)
file(GLOB imgui_SRCS ${d_deps}/imgui/*.cpp)
file(GLOB lua_SRCS   ${d_deps}/lua/*.c)

file(GLOB picoModS   ${gpl_deps}/picotcp/modules/*.c)
file(GLOB picoStkS   ${gpl_deps}/picotcp/stack/*.c)
set(pico_SRCS ${picoModS} ${picoStkS})

set(deps_SRCS
  ${dirent_SRCS}
  ${lz_SRCS}
  ${lpng_SRCS}
  ${lelf_SRCS}
  ${chdr_SRCS}
  ${crypt_SRCS}
  ${imgui_SRCS}
  ${lua_SRCS}
  ${gpl_deps}/xbrz/xbrz.cpp
  ${d_deps}/xxhash/xxhash.c
  ${d_deps}/cdipsr/cdipsr.cpp
  ${d_deps}/coreio/coreio.cpp
  ${xbyak_H}
)
  
if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC} OR ${BUILD_COMPILER} EQUAL ${COMPILER_CLANG})
  list(APPEND deps_SRCS
    ${lzip_SRCS}
    ${lzma_SRCS}
    ${pico_SRCS}
  )
  add_definitions(-D_7ZIP_ST -DCHD5_LZMA)
endif()

### libosd.cmake ################################################################################

set(d_aout ${reicast_core_path}/oslib)

include_directories ("${d_core}/khronos")

## I really should just glob all of the dirs and ;shrug; if guards don't do it all ##

set(osd_SRCS "")

list(APPEND osd_SRCS ${d_aout}/audiostream.cpp)

if (${HOST_OS} EQUAL ${OS_WINDOWS})

  list(APPEND osd_SRCS ${d_core}/windows/winmain.cpp)
  list(APPEND osd_SRCS ${d_core}/windows/win_vmem.cpp)
  list(APPEND osd_SRCS ${d_core}/oslib/windows/threading.cpp)
  list(APPEND osd_SRCS ${d_aout}/audiobackend_directsound.cpp)
  
  
  link_libraries(dsound.lib winmm.lib xinput.lib wsock32.lib opengl32.lib)

  
elseif (${HOST_OS} EQUAL ${OS_LINUX} OR ${HOST_OS} EQUAL ${OS_ANDROID})

    list(APPEND osd_SRCS
      ${d_core}/linux-dist/main.cpp
      ${d_core}/linux/common.cpp
      ${d_core}/linux/context.cpp
      ${d_core}/linux/posix_vmem.cpp
      ${d_core}/linux/nixprof/nixprof.cpp
      ${d_core}/oslib/posix/threading.cpp

      ${d_aout}/audiobackend_oss.cpp # add option
    ) # todo: configure linux audio lib options

elseif(${HOST_OS} EQUAL ${OS_DARWIN})

  set(d_osx ${reicast_shell_path}/apple/emulator-osx/emulator-osx)

  list(APPEND objc_SRCS
    ${d_osx}/main.m
    ${d_osx}/input.mm
    ${d_osx}/AppDelegate.mm
    ${d_osx}/EmuGLView.m
  )
  
  list(APPEND osd_SRCS ${objc_SRCS}
    ${d_core}/linux/common.cpp
    ${d_core}/linux/context.cpp
    ${d_core}/linux/posix_vmem.cpp
    ${d_core}/linux/nixprof/nixprof.cpp
    ${d_core}/oslib/posix/threading.cpp
    ${d_aout}/audiobackend_coreaudio.cpp
  )

else()

  message("OS Unhandled")
  error()

endif()

include_directories ("${reicast_core_path}")

set(reicast_SRCS ${core_SRCS} ${deps_SRCS} ${osd_SRCS})

add_executable(${TNAME}${binSuffix} ${reicast_SRCS} ${deps_SRCS}) 

target_compile_features(${TNAME}${binSuffix} PRIVATE cxx_std_14)

if(${HOST_OS} EQUAL ${OS_LINUX})
  # needed for opentty
  target_link_libraries(${TNAME} -lutil)

  find_package(ALSA)
  if(ALSA_FOUND)
    target_compile_definitions(${TNAME} PUBLIC USE_ALSA)
    target_sources(${TNAME} PUBLIC ${d_aout}/audiobackend_alsa.cpp)
    target_include_directories(${TNAME} PUBLIC ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${TNAME} ${ALSA_LIBRARIES})
  endif()

  find_package(OpenGL REQUIRED)
  if(OpenGL_EGL_FOUND)
    target_compile_definitions(${TNAME} PUBLIC SUPPORT_EGL)
    target_sources(${TNAME} PUBLIC ${d_core}/utils/glinit/egl/egl.cpp)
    target_link_libraries(${TNAME} OpenGL::EGL)
  elseif(OpenGL_GLX_FOUND)
    target_compile_definitions(${TNAME} PUBLIC SUPPORT_GLX)
    target_sources(${TNAME} PUBLIC ${d_core}/utils/glinit/glx/glx.cpp)
    target_link_libraries(${TNAME} OpenGL::GLX)
  else()
    message(FATAL_ERROR "EGL or GLX required")
  endif()

  find_package(OpenMP)
  if(OpenMP_FOUND)
    target_link_libraries(${TNAME} OpenMP::OpenMP_CXX)
  else()
    target_compile_definitions(${TNAME} PUBLIC TARGET_NO_OPENMP)
  endif()

  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(AO ao)
    if(AO_FOUND)
      target_compile_definitions(${TNAME} PUBLIC USE_LIBAO)
      target_sources(${TNAME} PUBLIC ${d_aout}/audiobackend_libao.cpp)
      target_include_directories(${TNAME} PUBLIC ${AO_INCLUDE_DIRS})
      target_link_libraries(${TNAME} ${AO_LIBRARIES})
    endif()

    pkg_check_modules(LIBEVDEV libevdev)
    if(LIBEVDEV_FOUND)
      target_compile_definitions(${TNAME} PUBLIC USE_EVDEV)
      target_sources(${TNAME} PUBLIC ${d_core}/linux-dist/evdev.cpp)
      target_include_directories(${TNAME} PUBLIC ${LIBEVDEV_INCLUDE_DIRS})
      target_link_libraries(${TNAME} ${LIBEVDEV_LIBRARIES})

      pkg_check_modules(LIBUDEV libudev)
      if(LIBUDEV_FOUND)
        target_compile_definitions(${TNAME} PUBLIC USE_UDEV)
        target_include_directories(${TNAME} PUBLIC ${LIBUDEV_INCLUDE_DIRS})
        target_link_libraries(${TNAME} ${LIBUDEV_LIBRARIES})
      endif()
    endif()

    pkg_check_modules(LIBPULSE_SIMPLE libpulse-simple)
    if(LIBPULSE_SIMPLE_FOUND)
      target_compile_definitions(${TNAME} PUBLIC USE_PULSEAUDIO)
      target_sources(${TNAME} PUBLIC ${d_aout}/audiobackend_pulseaudio.cpp)
      target_include_directories(${TNAME} PUBLIC ${LIBPULSE_SIMPLE_INCLUDE_DIRS})
      target_link_libraries(${TNAME} ${LIBPULSE_SIMPLE_LIBRARIES})
    endif()
  endif()

  find_package(Threads REQUIRED)
  target_link_libraries(${TNAME} Threads::Threads)

  find_package(X11 REQUIRED)
  if(X11_FOUND)
    target_sources(${TNAME} PUBLIC ${d_core}/linux-dist/x11.cpp)
    target_compile_definitions(${TNAME} PUBLIC SUPPORT_X11)
    target_include_directories(${TNAME} PUBLIC ${X11_INCLUDE_DIR})
    target_link_libraries(${TNAME} ${X11_LIBRARIES})
  endif()

  target_link_libraries(${TNAME} ${CMAKE_DL_LIBS} rt)
endif()

if(${HOST_OS} EQUAL ${OS_DARWIN})
  enable_language(OBJC)
  enable_language(OBJCXX)

  # Silence warnings about OpenGL deprecation on Mojave and greater (10.14+)
  target_compile_options(${TNAME} PRIVATE -DGL_SILENCE_DEPRECATION)

  # Enable ARC
  target_compile_options(${TNAME} PRIVATE -fobjc-arc)

  # Enable OpenMP
  target_compile_options(${TNAME} PRIVATE -Xpreprocessor -fopenmp)
  target_link_libraries(${TNAME} "${d_osx}/../deps/libomp/lib/libomp.a")
  target_include_directories(${TNAME} PRIVATE "${d_osx}/../deps/libomp/include")

  # Include macOS *.hpp files
  target_include_directories(${TNAME} PRIVATE "${d_osx}")
 
  # Link macOS Frameworks
  target_link_libraries(${TNAME}
    "-framework Cocoa"
    "-framework AppKit"
    "-framework CoreData"
    "-framework CoreAudio"
    "-framework AudioUnit"
    "-framework AudioToolbox"
    "-framework Foundation"
    "-framework OpenGL"
  )

  # Create the macOS app bundle
  add_custom_command(TARGET ${TNAME} POST_BUILD
    COMMAND bash "-c" "D_OSX=\"${d_osx}\" CMAKE_CURRENT_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\" CMAKE_CURRENT_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" \"${d_osx}/build_app_bundle.sh\""
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Creating macOS app bundle..."
  )

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



















