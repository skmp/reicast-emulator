## ps4sdk.cmake - devkitpro A64 cross-compile
#
set(CMAKE_SYSTEM_NAME FreeBSD) # this one is important
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 9)   # this one not so much



### This shit is very WIP ###
#
## TODO: Check for 


set(PS4SDK $ENV{PS4SDK})
  

  
if ("" STREQUAL "${PS4SDK}")
  if ("Windows" STREQUAL "${CMAKE_HOST_SYSTEM_NAME}")
    set(PS4SDK "C:/Dev/PS4SDK")
  else()
    set(PS4SDK "/opt/ps4/sdk")
  endif()
endif()


set(PS4HOST   ${PS4SDK}/host_tools)
set(PS4TARGET ${PS4SDK}/target)



set(toolPrefix "orbis-")
set(toolSuffix ".exe")

set(CMAKE_C_COMPILER   ${PS4HOST}/bin/${toolPrefix}clang${toolSuffix})
set(CMAKE_CXX_COMPILER ${PS4HOST}/bin/${toolPrefix}clang++${toolSuffix})


### IF NOT SCE SDK
#set(triple "x86_64-scei-ps4")
#set(CMAKE_C_COMPILER_TARGET ${triple})
#set(CMAKE_CXX_COMPILER_TARGET ${triple})


set(CMAKE_FIND_ROOT_PATH  ${PS4TARGET}) # where is the target environment

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) # search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)  # for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set (PS4_inc_dirs 
  ${PS4TARGET}/include 
  ${PS4TARGET}/include_common
  ${PS4TARGET}/tauon/include 
)

include_directories(${PS4_inc_dirs})


set (PS4_link_dirs
  "${PS4TARGET}/lib"
  "${PS4TARGET}/tauon/lib"
)


set(LINK_DIRECTORIES ${PS4_link_dirs})
#link_directories(${PS4_link_dirs})






set(TARGET_PS4 ON)
set(TARGET_BSD ON)






