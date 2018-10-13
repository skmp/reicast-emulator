##

include_directories ("${PROJECT_SOURCE_DIR}/core/deps")

set(d_p ./core/deps)

file(GLOB lz_SRCS   ${d_p}/zlib/*.c)
file(GLOB lws_SRCS  ${d_p}/libwebsocket/*.c)
file(GLOB lzip_SRCS ${d_p}/libzip/*.c)
file(GLOB lpng_SRCS ${d_p}/libpng/*.c)

set(deps_SRCS
  ${d_p}/cdipsr/cdipsr.cpp
  ${d_p}/chdr/chdr.cpp
  ${d_p}/coreio/coreio.cpp
  ${d_p}/crypto/md5.cpp
  ${d_p}/crypto/sha1.cpp
  ${d_p}/crypto/sha256.cpp
  
#  ${d_p}/ifaddrs/ifaddrs.c
  
  ${d_p}/libelf/elf32.cpp
  ${d_p}/libelf/elf64.cpp
  ${d_p}/libelf/elf.cpp
#
  ${d_p}/libpng/png.c		# todo readd arm opt 
  ${d_p}/libpng/pngget.c
  ${d_p}/libpng/pngmem.c
  ${d_p}/libpng/pngrio.c
  ${d_p}/libpng/pngrutil.c
  ${d_p}/libpng/pngwio.c
  ${d_p}/libpng/pngwutil.c
  ${d_p}/libpng/pngwrite.c
  ${d_p}/libpng/pngtrans.c
  ${d_p}/libpng/pngset.c
  ${d_p}/libpng/pngrtran.c
  ${d_p}/libpng/pngread.c
  ${d_p}/libpng/pngerror.c
  ${d_p}/libpng/pngwtran.c
#../core/deps/libpng/contrib/arm-neon: android-ndk.c  linux-auxv.c  linux.c
#
#  ${lws_SRCS}	#../core/deps/libwebsocket/
#
  ${lz_SRCS}
# ${lzip_SRCS}	#../core/deps/libzip:
#  
  )








#if(BUILD_LIBS)
#  add_library(libdeps STATIC ${deps_SRCS})
#else()
  list(APPEND reicast_SRCS ${deps_SRCS})
#endif()

