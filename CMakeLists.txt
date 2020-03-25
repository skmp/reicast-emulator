cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

project(reicast)

enable_language(ASM)
enable_language(ASM_MASM)
enable_language(C)
enable_language(CXX)

set(DEBUG_CMAKE ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Paths
set(reicast_root_path  "${CMAKE_CURRENT_SOURCE_DIR}")
set(reicast_core_path  "${reicast_root_path}/libswirl")
set(reicast_shell_path "${reicast_root_path}/reicast")

# Include config.cmake to set various values such as the HOST_OS
list(APPEND CMAKE_MODULE_PATH "${reicast_shell_path}/cmake")
include(config)			# configure build settings, must be first

# Load 
include(GetGitRevisionDescription)
git_describe(GIT_VERSION --tags)
configure_file(${reicast_core_path}/version.h.in ${reicast_core_path}/version.h @ONLY)

set(reicast_SRCS "")

################################################################################################
### libswirl library ###########################################################################
################################################################################################

set(t_libswirl "swirl")
add_library(${t_libswirl} STATIC)

# FIXME: some better way?
target_compile_definitions(${t_libswirl} PUBLIC -DSCRIPTING)

### core sources ###############################################################################

set(d_core ${reicast_core_path})

file(GLOB_RECURSE hw_SRCS       ${d_core}/hw/*.cpp           ${d_core}/hw/*.h)
file(GLOB_RECURSE lxdream_SRCS  ${d_core}/gpl/lxdream/*.cpp  ${d_core}/gpl/lxdream/*.h)

file(GLOB cfg_SRCS        ${d_core}/cfg/*.cpp        ${d_core}/cfg/*.h)
file(GLOB rend_SRCS       ${d_core}/rend/*.cpp       ${d_core}/rend/*.h)
file(GLOB input_SRCS      ${d_core}/input/*.cpp      ${d_core}/input/*.h)
file(GLOB reios_SRCS      ${d_core}/reios/*.cpp      ${d_core}/reios/*.h)
file(GLOB imgread_SRCS    ${d_core}/imgread/*.cpp    ${d_core}/imgread/*.h)
file(GLOB profiler_SRCS   ${d_core}/profiler/*.cpp   ${d_core}/profiler/*.h)
file(GLOB archive_SRCS    ${d_core}/archive/*.cpp    ${d_core}/archive/*.h)
file(GLOB scripting_SRCS  ${d_core}/scripting/*.cpp  ${d_core}/scripting/*.h)
file(GLOB utils_SRCS      ${d_core}/utils/*.cpp	     ${d_core}/utils/*.h)
file(GLOB gui_SRCS        ${d_core}/gui/*.cpp        ${d_core}/gui/*.h)
file(GLOB glwrap_SRCS     ${d_core}/utils/glwrap/*.cpp)
if(host_windows)
  file(GLOB wgl_SRCS  ${d_core}/utils/glinit/wgl/*.cpp )
endif()

if(NOT host_macos)
  file(GLOB gl4_SRCS     ${d_core}/rend/gl4/*.cpp     ${d_core}/rend/gl4/*.h)
endif()
file(GLOB gles_SRCS      ${d_core}/rend/gles/*.cpp    ${d_core}/rend/gles/*.h)
file(GLOB norend_SRCS    ${d_core}/rend/norend/*.cpp  ${d_core}/rend/norend/*.h)
file(GLOB softrend_SRCS  ${d_core}/rend/soft/*.cpp    ${d_core}/rend/soft/*.h)
if(host_windows)
  file(GLOB d3d11_SRCS   ${d_core}/rend/d3d11/*.cpp   ${d_core}/rend/d3d11/*.h)
endif()

set(lic_)

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

if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC} OR (${BUILD_COMPILER} EQUAL ${COMPILER_CLANG} AND host_macos)) # TODO: Test with Clang on other platforms
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
    list(APPEND core_SRCS 
      ${d_core}/jit/backend/x64/rec_x64.cpp 
      ${d_core}/jit/backend/x64/x64_regalloc.h
    )
  elseif(${HOST_CPU} EQUAL ${CPU_A64})
    list(APPEND core_SRCS 
      ${d_core}/rec-ARM64/rec_arm64.cpp 
      ${d_core}/rec-ARM64/arm64_regalloc.h
    )
  else()
    message("FEAT_SHREC==DYNAREC_JIT && HOST_CPU Unknown Default add arch or disable rec if not avail.")
	  error()
  endif()
elseif(${FEAT_SHREC} EQUAL ${DYNAREC_CPP})
    list(APPEND core_SRCS ${d_core}/jit/backend/cpp/rec_cpp.cpp)
endif()

if(host_linux)
  target_compile_definitions(${t_libswirl} PUBLIC -DLUA_USE_LINUX)
endif()

### deps sources ###############################################################################

set(d_deps "${reicast_core_path}/deps")
set(gpl_deps "${reicast_core_path}/gpl/deps")

target_include_directories(${t_libswirl} PUBLIC "${d_deps}")
target_include_directories(${t_libswirl} PUBLIC "${gpl_deps}/picotcp/include")
target_include_directories(${t_libswirl} PUBLIC "${gpl_deps}/picotcp/modules")

file(GLOB xbyak_H	${d_deps}/xbyak/*.h)		# include headers into cmake target/project view 

file(GLOB chdr_SRCS  ${d_deps}/chdr/*.c)
if(host_windows)
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
  
if(${BUILD_COMPILER} EQUAL ${COMPILER_GCC} OR (${BUILD_COMPILER} EQUAL ${COMPILER_CLANG} AND host_macos)) # TODO: Test with Clang on other platforms
  list(APPEND deps_SRCS
    ${lzip_SRCS}
    ${lzma_SRCS}
    ${pico_SRCS}
  )
  target_compile_definitions(${t_libswirl} PUBLIC -D_7ZIP_ST -DCHD5_LZMA)
endif()

### osd sources ################################################################################

set(d_aout ${reicast_core_path}/oslib)

target_include_directories(${t_libswirl} PUBLIC "${d_core}/khronos")

## I really should just glob all of the dirs and ;shrug; if guards don't do it all ##

set(osd_SRCS "")

list(APPEND osd_SRCS ${d_aout}/audiostream.cpp)

if(host_windows)
  list(APPEND reicast_SRCS ${d_core}/windows/winmain.cpp)
  list(APPEND osd_SRCS ${d_core}/windows/win_vmem.cpp)
  list(APPEND osd_SRCS ${d_core}/oslib/windows/threading.cpp)
  list(APPEND osd_SRCS ${d_aout}/audiobackend_directsound.cpp)
  link_libraries(dsound.lib winmm.lib xinput.lib wsock32.lib opengl32.lib)

elseif(host_linux OR host_android)
  list(APPEND reicast_SRCS ${d_core}/linux-dist/main.cpp)
  list(APPEND osd_SRCS
    #${d_core}/linux-dist/main.cpp
    ${d_core}/linux/common.cpp
    ${d_core}/linux/context.cpp
    ${d_core}/linux/posix_vmem.cpp
    ${d_core}/linux/nixprof/nixprof.cpp
    ${d_core}/oslib/posix/threading.cpp

    ${d_aout}/audiobackend_oss.cpp # add option
  ) # todo: configure linux audio lib options

elseif(host_macos)
  set(d_osx ${reicast_shell_path}/apple/emulator-osx/emulator-osx)

  list(APPEND reicast_SRCS
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

set(t_reicast "${PROJECT_NAME}${binSuffix}")
add_executable(${t_reicast}) 

target_include_directories(${t_libswirl} PRIVATE "${reicast_core_path}")
target_include_directories(${t_reicast} PRIVATE "${reicast_core_path}")

target_compile_features(${t_reicast} PRIVATE cxx_std_14)
target_compile_features(${t_libswirl} PRIVATE cxx_std_14)

if(host_linux)
  # needed for opentty
  target_link_libraries(${t_libswirl} -lutil)

  find_package(ALSA)
  if(ALSA_FOUND)
    target_compile_definitions(${t_libswirl} PUBLIC USE_ALSA)
    target_sources(${t_libswirl} PUBLIC ${d_aout}/audiobackend_alsa.cpp)
    target_include_directories(${t_libswirl} PUBLIC ${ALSA_INCLUDE_DIRS})
    target_link_libraries(${t_libswirl} ${ALSA_LIBRARIES})
  endif()

  find_package(OpenGL REQUIRED)
  if(OpenGL_EGL_FOUND)
    target_compile_definitions(${t_libswirl} PUBLIC SUPPORT_EGL)
    target_sources(${t_libswirl} PUBLIC ${d_core}/utils/glinit/egl/egl.cpp)
    target_link_libraries(${t_libswirl} OpenGL::EGL)
  elseif(OpenGL_GLX_FOUND)
    target_compile_definitions(${t_libswirl} PUBLIC SUPPORT_GLX)
    target_sources(${t_libswirl} PUBLIC ${d_core}/utils/glinit/glx/glx.cpp)
    target_link_libraries(${t_libswirl} OpenGL::GLX)
  else()
    message(FATAL_ERROR "EGL or GLX required")
  endif()

  find_package(OpenMP)
  if(OpenMP_FOUND)
    target_link_libraries(${t_libswirl} OpenMP::OpenMP_CXX)
  else()
    target_compile_definitions(${t_libswirl} PUBLIC TARGET_NO_OPENMP)
  endif()

  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(AO ao)
    if(AO_FOUND)
      target_compile_definitions(${t_libswirl} PUBLIC USE_LIBAO)
      target_sources(${t_libswirl} PUBLIC ${d_aout}/audiobackend_libao.cpp)
      target_include_directories(${t_libswirl} PUBLIC ${AO_INCLUDE_DIRS})
      target_link_libraries(${t_libswirl} ${AO_LIBRARIES})
    endif()

    pkg_check_modules(LIBEVDEV libevdev)
    if(LIBEVDEV_FOUND)
      target_compile_definitions(${t_libswirl} PUBLIC USE_EVDEV)
      target_sources(${t_libswirl} PUBLIC ${d_core}/linux-dist/evdev.cpp)
      target_include_directories(${t_libswirl} PUBLIC ${LIBEVDEV_INCLUDE_DIRS})
      target_link_libraries(${t_libswirl} ${LIBEVDEV_LIBRARIES})

      pkg_check_modules(LIBUDEV libudev)
      if(LIBUDEV_FOUND)
        target_compile_definitions(${t_libswirl} PUBLIC USE_UDEV)
        target_include_directories(${t_libswirl} PUBLIC ${LIBUDEV_INCLUDE_DIRS})
        target_link_libraries(${t_libswirl} ${LIBUDEV_LIBRARIES})
      endif()
    endif()

    pkg_check_modules(LIBPULSE_SIMPLE libpulse-simple)
    if(LIBPULSE_SIMPLE_FOUND)
      target_compile_definitions(${t_libswirl} PUBLIC USE_PULSEAUDIO)
      target_sources(${t_libswirl} PUBLIC ${d_aout}/audiobackend_pulseaudio.cpp)
      target_include_directories(${t_libswirl} PUBLIC ${LIBPULSE_SIMPLE_INCLUDE_DIRS})
      target_link_libraries(${t_libswirl} ${LIBPULSE_SIMPLE_LIBRARIES})
    endif()
  endif()

  find_package(Threads REQUIRED)
  target_link_libraries(${t_libswirl} Threads::Threads)

  find_package(X11 REQUIRED)
  if(X11_FOUND)
    target_sources(${t_libswirl} PUBLIC ${d_core}/linux-dist/x11.cpp)
    target_compile_definitions(${t_libswirl} PUBLIC SUPPORT_X11)
    target_include_directories(${t_libswirl} PUBLIC ${X11_INCLUDE_DIR})
    target_link_libraries(${t_libswirl} ${X11_LIBRARIES})
  endif()

  target_link_libraries(${t_libswirl} ${CMAKE_DL_LIBS} rt)
endif()

if(host_macos)
  enable_language(OBJC)
  enable_language(OBJCXX)

  # Silence warnings about OpenGL deprecation on Mojave and greater (10.14+)
  target_compile_options(${t_reicast} PUBLIC -DGL_SILENCE_DEPRECATION)
  target_compile_options(${t_libswirl} PUBLIC -DGL_SILENCE_DEPRECATION)

  # Enable ARC
  target_compile_options(${t_reicast} PUBLIC -fobjc-arc)

  # Enable OpenMP
  target_compile_options(${t_libswirl} PUBLIC -Xpreprocessor -fopenmp)
  # target_link_libraries(${t_libswirl} "${d_osx}/../deps/libomp/lib/libomp.a")
  target_include_directories(${t_libswirl} PUBLIC "${d_osx}/../deps/libomp/include")
  message("D_OSX=\"${d_osx}\" CMAKE_CURRENT_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" \"${d_osx}/../../merge_static_libs.sh\"")
  add_custom_command(TARGET ${t_libswirl} POST_BUILD
    COMMAND bash "-c" "D_OSX=\"${d_osx}\" CMAKE_CURRENT_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" \"${d_osx}/../../merge_static_libs.sh\""
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Merging static libraries..."
  )

  # Include macOS *.hpp files
  target_include_directories(${t_reicast} PUBLIC "${d_osx}")
 
  # Link macOS Frameworks
  target_link_libraries(${t_reicast}
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
  add_custom_command(TARGET ${t_reicast} POST_BUILD
    COMMAND bash "-c" "D_OSX=\"${d_osx}\" CMAKE_CURRENT_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\" CMAKE_CURRENT_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\" \"${d_osx}/../../build_app_bundle.sh\""
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Creating macOS app bundle..."
  )
endif()

set(libswirl_SRCS ${core_SRCS} ${deps_SRCS} ${osd_SRCS})
target_sources(${t_libswirl} PRIVATE ${libswirl_SRCS})

target_sources(${t_reicast} PUBLIC ${reicast_SRCS})
add_dependencies(${t_reicast} ${t_libswirl})

if(${BUILD_COMPILER} EQUAL ${COMPILER_CLANG})
  #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -force_load ${CMAKE_CURRENT_BINARY_DIR}/lib${t_libswirl}.a")
  target_link_libraries(${t_reicast} -force_load ${CMAKE_CURRENT_BINARY_DIR}/lib${t_libswirl}.a)
elseif(${BUILD_COMPILER} EQUAL ${COMPILER_GCC})
  #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/lib${t_libswirl}.a -Wl,--no-whole-archive")
  target_link_libraries(${t_reicast} -Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/lib${t_libswirl}.a -Wl,--no-whole-archive)
endif()

# aica-dsp-asm
# if(host_macos)
#   add_executable(aica-dsp-asm
#     aica-dsp-asm/main.cpp
#     aica-dsp-asm/macos_support.mm
#   )

#   # Link macOS Frameworks
#   target_link_libraries(aica-dsp-asm "-framework Foundation")
# endif()


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



















