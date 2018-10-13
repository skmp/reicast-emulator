## libdreamcast module 
#
if(NOT RE_CMAKE_CONFIGURED)
error("include config.cmake before libs")
endif()


set(libdc_base_path "${PROJECT_SOURCE_DIR}/core/dc_arch")

include_directories ("${libdc_base_path}")



message("libDreamcast: base path ${libdc_base_path}")






## Set Base then append 
#
set(libdc_SRCS 
#
 ./core/libdreamcast.cpp
 
 ./core/profiler/profiler.cpp
 
 ./core/reios/descrambl.cpp
 ./core/reios/gdrom_hle.cpp
 ./core/reios/reios.cpp
 ./core/reios/reios_elf.cpp

 ./core/dc_arch/aica/aica.cpp 
 ./core/dc_arch/aica/aica_if.cpp 
 ./core/dc_arch/aica/aica_mem.cpp 
 ./core/dc_arch/aica/dsp.cpp 
 ./core/dc_arch/aica/sgc_if.cpp
 
 ./core/dc_arch/arm7/arm7.cpp
 ./core/dc_arch/arm7/arm_mem.cpp
 ./core/dc_arch/arm7/vbaARM.cpp 
 ./core/dc_arch/arm7/virt_arm.cpp
 
 ./core/dc_arch/gdrom/gdromv3.cpp 
 ./core/dc_arch/gdrom/gdrom_response.cpp
 
 ./core/dc_arch/holly/holly_intc.cpp  
 ./core/dc_arch/holly/sb.cpp  
 ./core/dc_arch/holly/sb_dma.cpp  
 ./core/dc_arch/holly/sb_mem.cpp
 
 ./core/dc_arch/maple/maple_cfg.cpp  
 ./core/dc_arch/maple/maple_devs.cpp  
 ./core/dc_arch/maple/maple_helper.cpp  
 ./core/dc_arch/maple/maple_if.cpp
 
 ./core/dc_arch/mem/_vmem.cpp
 
 ./core/dc_arch/naomi/naomi_cart.cpp  
 ./core/dc_arch/naomi/naomi.cpp
 
 ./core/dc_arch/pvr/drkPvr.cpp 
 ./core/dc_arch/pvr/pvr_regs.cpp 
 ./core/dc_arch/pvr/Renderer_if.cpp 
 ./core/dc_arch/pvr/ta.cpp 
 ./core/dc_arch/pvr/ta_vtx.cpp 
 ./core/dc_arch/pvr/pvr_mem.cpp 
 ./core/dc_arch/pvr/pvr_sb_regs.cpp 
 ./core/dc_arch/pvr/spg.cpp 
 ./core/dc_arch/pvr/ta_ctx.cpp
 
 ./core/dc_arch/sh4/sh4_interrupts.cpp 
 ./core/dc_arch/sh4/sh4_mmr.cpp 
 ./core/dc_arch/sh4/sh4_rom.cpp 
 ./core/dc_arch/sh4/sh4_core_regs.cpp
 ./core/dc_arch/sh4/sh4_mem.cpp
 ./core/dc_arch/sh4/sh4_opcode_list.cpp
 ./core/dc_arch/sh4/sh4_sched.cpp
 
 ./core/dc_arch/sh4/dyna/blockmanager.cpp
 ./core/dc_arch/sh4/dyna/decoder.cpp
 ./core/dc_arch/sh4/dyna/driver.cpp
 ./core/dc_arch/sh4/dyna/shil.cpp
 
 ./core/dc_arch/sh4/interpr/sh4_opcodes.cpp
 ./core/dc_arch/sh4/interpr/sh4_interpreter.cpp
 ./core/dc_arch/sh4/interpr/sh4_fpu.cpp
 
 ./core/dc_arch/sh4/modules/ccn.cpp
 ./core/dc_arch/sh4/modules/dmac.cpp
 ./core/dc_arch/sh4/modules/mmu.cpp
 ./core/dc_arch/sh4/modules/bsc.cpp
 ./core/dc_arch/sh4/modules/intc.cpp
 ./core/dc_arch/sh4/modules/tmu.cpp
 ./core/dc_arch/sh4/modules/cpg.cpp
 ./core/dc_arch/sh4/modules/rtc.cpp
 ./core/dc_arch/sh4/modules/serial.cpp
 ./core/dc_arch/sh4/modules/ubc.cpp
#
)


if(${FEAT_SHREC} EQUAL ${DYNAREC_CPP})
    list(APPEND libdc_SRCS ./core/dynarec/cpp/rec_cpp.cpp)
endif()

if(${FEAT_SHREC} EQUAL ${DYNAREC_JIT})
#
  if(${HOST_CPU} EQUAL ${CPU_X86})
    list(APPEND libdc_SRCS 
      ./core/dynarec/x86/rec_x86_driver.cpp
      ./core/dynarec/x86/rec_x86_il.cpp
      ./core/dynarec/x86/rec_x86_asm.cpp	# change for linux , rec_lin86_asm.S
     )
  elseif(${HOST_CPU} EQUAL ${CPU_X64})
    list(APPEND libdc_SRCS ./core/dynarec/x64/rec_x64.cpp)
  elseif(${HOST_CPU} EQUAL ${CPU_ARM} OR 
		 ${HOST_CPU} EQUAL ${CPU_A64})
    list(APPEND libdc_SRCS 
      ./core/dynarec/arm/ngen_arm.S
      ./core/dynarec/arm/rec_arm.cpp
	)
  else()
    error(" SHREC Default , prob aarch64 , add or disable rec if not avail.")
  endif()
#
endif()





if(NOT ON) #DEBUG_CMAKE)
message("Adding libdreamcast ... \n\n >> libdc_SRCS: \n{\n")
foreach(_entry_t IN LISTS libdc_SRCS)  # see not required here 
  message("${_entry_t}")
endforeach()
message("}\n >> End libdc_SRCS: \n\n")
endif()




if(BUILD_LIBS)
  add_library(libdreamcast ${BUILD_LIB_TYPE} ${libdc_SRCS})
else()
  list(APPEND reicast_SRCS ${libdc_SRCS})
endif()




if (NOT ${HOST_OS} EQUAL ${OS_NSW_HOS})
  source_group(TREE ${PROJECT_SOURCE_DIR}/core PREFIX src\\libdreamcast FILES ${libdc_SRCS})
endif()


#set_property(TARGET libdreamcast PROPERTY IMPORTED_LOCATION $...)





















#[[

## add_src_group(group_name base_dir LIST(files))
#
#   calls source_group() 
#
macro(add_src_group name dir_base _t_file_list)
#
  message(" @@@ add_src_group ${name} ${dir_base} ")

  foreach(_t_file IN LISTS ${_t_file_list})	# please tell me why lists have to be dereferenced here but not elsewhere?? #
	list(APPEND libdc_SRCS "${dir_base}/${_t_file}")
  endforeach()
  
  source_group(${name} FILES ${_t_file_list})
#
endmacro()








######### maybe will replace w. source_group(TREE <root> PREFIX sources\\inc ...)



## add groups for each section,  for people who use native IDE support *TODO* check this out further
#

set(_dc_arch_src aica.cpp aica_if.cpp aica_mem.cpp dsp.cpp sgc_if.cpp)
set(_dc_arch_h   aica.h  aica_if.h  aica_mem.h  dsp.h  sgc_if.h)
add_src_group("dc_arch\\aica" "./core/dc_arch/aica" _dc_arch_src)

set(_dc_arch_src arm7.cpp  arm_mem.cpp  vbaARM.cpp  virt_arm.cpp)
set(_dc_arch_h   arm7.h  arm_mem.h  arm-new.h  resource.h  virt_arm.h)
add_src_group("dc_arch\\arm7" "./core/dc_arch/arm7"  _dc_arch_src)

set(_dc_arch_h  flashrom.h)
#add_src_group("dc_arch/flashrom" "./core/dc_arch/flashrom" _dc_arch_h)

set(_dc_arch_src gdromv3.cpp gdrom_response.cpp)
set(_dc_arch_h   gdromv3.h gdrom_if.h)
add_src_group("dc_arch\\gdrom" "./core/dc_arch/gdrom" _dc_arch_src)

set(_dc_arch_src holly_intc.cpp  sb.cpp  sb_dma.cpp  sb_mem.cpp)
set(_dc_arch_h   holly_intc.h  sb.h  sb_mem.h)
add_src_group("dc_arch\\holly" "./core/dc_arch/holly" _dc_arch_src)

set(_dc_arch_src maple_cfg.cpp  maple_devs.cpp  maple_helper.cpp  maple_if.cpp)
set(_dc_arch_h   maple_cfg.h  maple_devs.h  maple_helper.h  maple_if.h)
add_src_group("dc_arch\\maple" "./core/dc_arch/maple" _dc_arch_src)

set(_dc_arch_src _vmem.cpp)
set(_dc_arch_h   _vmem.h)
add_src_group("dc_arch\\mem" "./core/dc_arch/mem" _dc_arch_src)

set(_dc_arch_src naomi_cart.cpp  naomi.cpp)
set(_dc_arch_h   naomi_cart.h  naomi.h  naomi_regs.h)
add_src_group("dc_arch\\naomi" "./core/dc_arch/naomi" _dc_arch_src)

set(_dc_arch_src drkPvr.cpp pvr_regs.cpp Renderer_if.cpp ta.cpp ta_vtx.cpp pvr_mem.cpp pvr_sb_regs.cpp spg.cpp ta_ctx.cpp)
set(_dc_arch_h   drkPvr.h config.h pvr_mem.h pvr_sb_regs.h pvr_regs.h spg.h helper_classes.h Renderer_if.h ta.h ta_const_df.h ta_ctx.h ta_structs.h)
add_src_group("dc_arch\\pvr" "./core/dc_arch/pvr" _dc_arch_src)

set(_dc_arch_src sh4_interrupts.cpp sh4_mmr.cpp sh4_rom.cpp sh4_core_regs.cpp sh4_mem.cpp sh4_opcode_list.cpp sh4_sched.cpp)
set(_dc_arch_h   sh4_interrupts.h fsca-table.h sh4_if.h sh4_mmr.h sh4_opcode_list.h sh4_sched.h sh4_core.h sh4_interpreter.h sh4_mem.h sh4_opcode.h sh4_rom.h)
add_src_group("dc_arch\\sh4" "./core/dc_arch/sh4" _dc_arch_src)

set(_dc_arch_src blockmanager.cpp decoder.cpp driver.cpp shil.cpp)
set(_dc_arch_h   blockmanager.h decoder.h decoder_opcodes.h ngen.h rec_config.h regalloc.h shil_canonical.h shil.h)
add_src_group("dc_arch\\sh4\\dyna" "./core/dc_arch/sh4/dyna" _dc_arch_src)

set(_dc_arch_src sh4_opcodes.cpp sh4_interpreter.cpp sh4_fpu.cpp)
set(_dc_arch_h   sh4_opcodes.h)
add_src_group("dc_arch\\sh4\\interpr" "./core/dc_arch/sh4/interpr" _dc_arch_src)

set(_dc_arch_src ccn.cpp dmac.cpp mmu.cpp bsc.cpp  intc.cpp  tmu.cpp cpg.cpp  rtc.cpp  serial.cpp   ubc.cpp)
set(_dc_arch_h   ccn.h  dmac.h  mmu.h  mmu_impl.h  modules.h  tmu.h)
add_src_group("dc_arch\\sh4\\modules" "./core/dc_arch/sh4/modules" _dc_arch_src)



#]]









