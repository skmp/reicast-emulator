cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)


    if(HOST_OS EQUAL OS_WINDOWS)

	add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)

elseif(HOST_OS EQUAL OS_LINUX)

	add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)

elseif(HOST_OS EQUAL OS_ANDROID)

	add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)

elseif(HOST_OS EQUAL OS_DARWIN)

	add_definitions(-DVK_USE_PLATFORM_MACOS_MVK)

else()

	message("Unsupported HOST_OS==${HOST_OS} for Vulkan!")
	error()

endif()

include_directories(${reicast_core_path}/hw/pvr)


set(d_vk ${reicast_core_path}/rend/vulkan)

set(vk_SRCS 
	${d_vk}/vk_base.h
	${d_vk}/vk_renderer.cpp
)
#PARENT_SCOPE)
	




























