# vktools.cmake
#

if(WIN32)
	set(_BSUFF ".exe")
endif()


if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
	set(GLSL_COMPILER  "$ENV{VULKAN_SDK}/Bin/glslc${_BSUFF}") # CACHE STRING "Which glsl compiler to use")
	set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator${_BSUFF}")
else()
	set(GLSL_COMPILER  "$ENV{VULKAN_SDK}/Bin32/glslc${_BSUFF}") # CACHE STRING "Which glsl compiler to use")
	set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator${_BSUFF}")
endif()






# Compiles the given GLSL shader through glslc.
function(compile_glslc shader output_file)
  get_filename_component(input_file ${shader} ABSOLUTE)
  add_custom_command (
	PRE_BUILD 
    OUTPUT ${output_file}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling SPIR-V binary ${shader}"
    DEPENDS ${shader} ${FILE_DEPS}
#   COMMAND ${GLSL_COMPILER} -mfmt=bin -o ${output_file} -c ${input_file} -MD	# --target-env=vulkan1.1 
	COMMAND ${GLSL_VALIDATOR} -V -o ${output_file} ${input_file}
  )
endfunction(compile_glslc)



function(compile_shaders src_dir dst_dir)
#
	file(GLOB shader_SRCS ${src_dir}/*)
	
	foreach(shader ${shader_SRCS})
	

		get_filename_component(file_name ${shader} NAME)
		get_filename_component(file_path_in ${shader} ABSOLUTE)
		set(file_path_out ${dst_dir}/${file_name}.spv)

		compile_glslc(${file_path_in} ${file_path_out})
		
		list(APPEND SHADER_BINARY_FILES ${file_path_out})
	endforeach()
  message("******compile_shaders*********** BIN FILES: ${SHADER_BINARY_FILES}")
  add_custom_target(Shaders ALL DEPENDS ${SHADER_BINARY_FILES})
#
endfunction(compile_shaders)








