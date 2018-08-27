// serialize.cpp : save states

#include "types.h"
#include "hw/aica/dsp.h"
#include "hw/aica/aica.h"
#include "hw/aica/sgc_if.h"
#include "hw/holly/sb_mem.h"
#include "hw/flashrom/flashrom.h"
#include "hw/mem/_vmem.h"
#include "hw/gdrom/gdromv3.h"
#include "hw/maple/maple_devs.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta_structs.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/mmu.h"
#include "imgread/common.h"
#include "reios/reios.h"
#include <map>
#include <set>
#include "rend/gles/gles.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/dyna/ngen.h"

/*
 * search for "maybe" to find items that were left out that may be needed
 */

enum serialize_version_enum {
	V1
} ;

//./core/hw/arm7/arm_mem.cpp
extern bool aica_interr;
extern u32 aica_reg_L;
extern bool e68k_out;
extern u32 e68k_reg_L;
extern u32 e68k_reg_M;



//./core/hw/arm7/arm7.cpp
extern DECL_ALIGN(8) reg_pair arm_Reg[RN_ARM_REG_COUNT];
extern bool armIrqEnable;
extern bool armFiqEnable;
extern int armMode;
extern bool Arm7Enabled;
extern u8 cpuBitsSet[256];
extern bool intState ;
extern bool stopState ;
extern bool holdState ;
/*
	if AREC dynarec enabled:
	vector<ArmDPOP> ops;
	u8* icPtr;
	u8* ICache;
	u8 ARM7_TCB[ICacheSize+4096];
	void* EntryPoints[8*1024*1024/4];
	map<u32,u32> renamed_regs;
	u32 rename_reg_base;
	u32 nfb,ffb,bfb,mfb;
	static x86_block* x86e;
	x86_Label* end_lbl;
	u8* ARM::emit_opt=0;
	eReg ARM::reg_addr;
	eReg ARM::reg_dst;
	s32 ARM::imma;
*/




//./core/hw/aica/dsp.o
extern DECL_ALIGN(4096) dsp_t dsp;
//recheck dsp.cpp if FEAT_DSPREC == DYNAREC_JIT




//./core/hw/aica/aica.o
//these are all just pointers into aica_reg
//extern CommonData_struct* CommonData;
//extern DSPData_struct* DSPData;
//extern InterruptInfo* MCIEB;
//extern InterruptInfo* MCIPD;
//extern InterruptInfo* MCIRE;
//extern InterruptInfo* SCIEB;
//extern InterruptInfo* SCIPD;
//extern InterruptInfo* SCIRE;

extern AicaTimer timers[3];



//./core/hw/aica/aica_if.o
extern VArray2 aica_ram;
extern u32 VREG;//video reg =P
extern u32 ARMRST;//arm reset reg
extern u32 rtc_EN;
extern int dma_sched_id;



//./core/hw/aica/aica_mem.o
extern u8 aica_reg[0x8000];



//./core/hw/aica/sgc_if.o
struct ChannelEx;
#define AicaChannel ChannelEx
extern s32 volume_lut[16];
extern s32 tl_lut[256 + 768];	//xx.15 format. >=255 is muted
extern u32 AEG_ATT_SPS[64];
extern u32 AEG_DSR_SPS[64];
extern s16 pl;
extern s16 pr;
//this is just a pointer to aica_reg
//extern DSP_OUT_VOL_REG* dsp_out_vol;
//not needed - one-time init
//void(* 	STREAM_STEP_LUT [5][2][2])(ChannelEx *ch)
//void(* 	STREAM_INITAL_STEP_LUT [5])(ChannelEx *ch)
//void(* 	AEG_STEP_LUT [4])(ChannelEx *ch)
//void(* 	FEG_STEP_LUT [4])(ChannelEx *ch)
//void(* 	ALFOWS_CALC [4])(ChannelEx *ch)
//void(* 	PLFOWS_CALC [4])(ChannelEx *ch)

//special handling
//extern AicaChannel AicaChannel::Chans[64];
#define Chans AicaChannel::Chans
#define CDDA_SIZE  (2352/2)
extern s16 cdda_sector[CDDA_SIZE];
extern u32 cdda_index;
extern SampleType mxlr[64];
extern u32 samples_gen;





//./core/hw/holly/sb.o
extern Array<RegisterStruct> sb_regs;
extern u32 SB_ISTNRM;
extern u32 SB_FFST_rc;
extern u32 SB_FFST;



//./core/hw/holly/sb_mem.o
//unused
//static HollyInterruptID dmatmp1;
//static HollyInterruptID dmatmp2;
//static HollyInterruptID OldDmaId;

//this is one-time init, no updates - don't need to serialize
//extern RomChip sys_rom;
extern SRamChip sys_nvmem_sram;
extern DCFlashChip sys_nvmem_flash;
//this is one-time init, no updates - don't need to serialize
//extern _vmem_handler area0_handler;




//./core/hw/gdrom/gdrom_response.o
extern u16 reply_11[] ;




//./core/hw/gdrom/gdromv3.o
extern int gdrom_sched;
extern signed int sns_asc;
extern signed int sns_ascq;
extern signed int sns_key;
extern packet_cmd_t packet_cmd;
extern u32 set_mode_offset;
extern read_params_t read_params ;
extern packet_cmd_t packet_cmd;
//Buffer for sector reads [dma]
extern read_buff_t read_buff ;
//pio buffer
extern pio_buff_t pio_buff ;
extern u32 set_mode_offset;
extern ata_cmd_t ata_cmd ;
extern cdda_t cdda ;
extern gd_states gd_state;
extern DiscType gd_disk_type;
extern u32 data_write_mode;
//Registers
extern u32 DriveSel;
extern GD_ErrRegT Error;
extern GD_InterruptReasonT IntReason;
extern GD_FeaturesT Features;
extern GD_SecCountT SecCount;
extern GD_SecNumbT SecNumber;
extern GD_StatusT GDStatus;
extern ByteCount_t ByteCount ;
extern int GDROM_TICK;





//./core/hw/maple/maple_devs.o
extern char EEPROM[0x100];
extern bool EEPROM_loaded;
extern _NaomiState State;
/*
 * each maple_device has unique sub-elements - need special handler
struct maple_base: maple_device
{
	u8* dma_buffer_out;
	u32* dma_count_out;

	u8* dma_buffer_in;
	u32 dma_count_in;

struct maple_sega_vmu: maple_base
{
	FILE* file;
	u8 flash_data[128*1024];
	u8 lcd_data[192];
	u8 lcd_data_decoded[48*32];

struct maple_device
{
	u8 maple_port;          //raw maple port
	u8 bus_port;            //0 .. 5
	u8 bus_id;              //0 .. 3
	wchar logical_port[3];  //A0, etc
	IMapleConfigMap* config;
struct maple_microphone: maple_base

	u8 micdata[SIZE_OF_MIC_DATA];
struct maple_sega_purupuru : maple_base

   u16 AST, AST_ms;
   u32 VIBSET;
*/




//./core/hw/maple/maple_if.o
//needs special handler
extern maple_device* MapleDevices[4][6];
//one time set
//extern int maple_sched;
//incremented but never read
//extern u32 dmacount;
extern bool maple_ddt_pending_reset;





//./core/hw/pvr/Renderer_if.o
//only written - not read
//extern u32 VertexCount;
extern u32 FrameCount;
//one-time init
//extern Renderer* renderer;
//these are just mutexes used during rendering
//extern cResetEvent rs;
//extern cResetEvent re;
//these max_?? through ovrn are written and not read
//extern int max_idx;
//extern int max_mvo;
//extern int max_op;
//extern int max_pt;
//extern int max_tr;
//extern int max_vtx;
//extern int max_modt;
//extern int ovrn;
//seems safe to omit this - gets refreshed every frame from a pool
//extern TA_context* _pvrrc;
//just a flag indiating the rendering thread is running
//extern int rend_en ;
//the renderer thread - one time set
//extern cThread rthd;
extern bool pend_rend;

//these will all get cleared out after a few frames - no need to serialize
//static bool render_called = false;
//u32 fb1_watch_addr_start;
//u32 fb1_watch_addr_end;
//u32 fb2_watch_addr_start;
//u32 fb2_watch_addr_end;
//bool fb_dirty;




//./core/hw/pvr/pvr_mem.o
extern u32 YUV_tempdata[512/4];//512 bytes
extern u32 YUV_dest;
extern u32 YUV_blockcount;
extern u32 YUV_x_curr;
extern u32 YUV_y_curr;
extern u32 YUV_x_size;
extern u32 YUV_y_size;




//./core/hw/pvr/pvr_regs.o
extern bool fog_needs_update;
extern u8 pvr_regs[pvr_RegSize];




//./core/hw/pvr/spg.o
extern u32 in_vblank;
extern u32 clc_pvr_scanline;
extern u32 pvr_numscanlines;
extern u32 prv_cur_scanline;
extern u32 vblk_cnt;
extern u32 Line_Cycles;
extern u32 Frame_Cycles;
extern int render_end_sched;
extern int vblank_sched;
extern int time_sync;
extern double speed_load_mspdf;
extern int mips_counter;
extern double full_rps;




//./core/hw/pvr/ta.o
extern u32 ta_type_lut[256];
extern u8 ta_fsm[2049];	//[2048] stores the current state
extern u32 ta_fsm_cl;




//./core/hw/pvr/ta_ctx.o
//these frameskipping don't need to be saved
//extern int frameskip;
//extern bool FrameSkipping;		// global switch to enable/disable frameskip
//maybe need these - but hopefully not
//extern TA_context* ta_ctx;
//extern tad_context ta_tad;
//extern TA_context*  vd_ctx;
//extern rend_context vd_rc;
//extern cMutex mtx_rqueue;
//extern TA_context* rqueue;
//extern cResetEvent frame_finished;
//extern cMutex mtx_pool;
//extern vector<TA_context*> ctx_pool;
//extern vector<TA_context*> ctx_list;
//end maybe



//./core/hw/pvr/ta_vtx.o
extern bool pal_needs_update;
extern u32 _pal_rev_256[4];
extern u32 _pal_rev_16[64];
extern u32 pal_rev_256[4];
extern u32 pal_rev_16[64];
//extern u32 palette_ram[1024];
extern u32 decoded_colors[3][65536];
extern u32 tileclip_val;
extern u8 f32_su8_tbl[65536];
//written but never read
//extern ModTriangle* lmr;
//never changed
//extern PolyParam nullPP;
//maybe
//extern PolyParam* CurrentPP;
//maybe
//extern List<PolyParam>* CurrentPPlist;
//TA state vars
extern DECL_ALIGN(4) u8 FaceBaseColor[4];
extern DECL_ALIGN(4) u8 FaceOffsColor[4];
#ifdef HAVE_OIT
extern DECL_ALIGN(4) u8 FaceBaseColor1[4];
extern DECL_ALIGN(4) u8 FaceOffsColor1[4];
#endif
extern DECL_ALIGN(4) u32 SFaceBaseColor;
extern DECL_ALIGN(4) u32 SFaceOffsColor;
//maybe
//extern TaListFP* TaCmd;
//maybe
//extern u32 CurrentList;
//maybe
//extern TaListFP* VertexDataFP;
//written but never read
//extern bool ListIsFinished[5];
//maybe ; need special handler
//FifoSplitter<0> TAFifo0;
//counter for frameskipping - doesn't need to be saved
//extern int ta_parse_cnt;




//./core/rend/TexCache.o
//maybe
//extern u8* vq_codebook;
extern u32 palette_index;
extern bool KillTex;
extern u32 palette16_ram[1024];
extern u32 palette32_ram[1024];
extern u32 detwiddle[2][8][1024];
//maybe
//extern vector<vram_block*> VramLocks[/*VRAM_SIZE*/(16*1024*1024)/PAGE_SIZE];
//maybe - probably not - just a locking mechanism
//extern cMutex vramlist_lock;
extern VArray2 vram;




//./core/hw/sh4/sh4_mmr.o
extern Array<u8> OnChipRAM;
extern Array<RegisterStruct> CCN;  //CCN  : 14 registers
extern Array<RegisterStruct> UBC;   //UBC  : 9 registers
extern Array<RegisterStruct> BSC;  //BSC  : 18 registers
extern Array<RegisterStruct> DMAC; //DMAC : 17 registers
extern Array<RegisterStruct> CPG;   //CPG  : 5 registers
extern Array<RegisterStruct> RTC;  //RTC  : 16 registers
extern Array<RegisterStruct> INTC;  //INTC : 4 registers
extern Array<RegisterStruct> TMU;  //TMU  : 12 registers
extern Array<RegisterStruct> SCI;   //SCI  : 8 registers
extern Array<RegisterStruct> SCIF; //SCIF : 10 registers




//./core/hw/sh4/sh4_mem.o
extern VArray2 mem_b;
//one-time init
//extern _vmem_handler area1_32b;
//one-time init
//extern _vmem_handler area5_handler;




//./core/hw/sh4/sh4_interrupts.o
extern u16 IRLPriority;
//one-time init
//extern InterptSourceList_Entry InterruptSourceList[28];
extern DECL_ALIGN(64) u16 InterruptEnvId[32];
extern DECL_ALIGN(64) u32 InterruptBit[32];
extern DECL_ALIGN(64) u32 InterruptLevelBit[16];
extern u32 interrupt_vpend; // Vector of pending interrupts
extern u32 interrupt_vmask; // Vector of masked interrupts             (-1 inhibits all interrupts)
extern u32 decoded_srimask; // Vector of interrupts allowed by SR.IMSK (-1 inhibits all interrupts)




//./core/hw/sh4/sh4_core_regs.o
extern Sh4RCB* p_sh4rcb;
//just method pointers
//extern sh4_if  sh4_cpu;
//one-time set
//extern u8* sh4_dyna_rcb;
extern u32 old_rm;
extern u32 old_dn;




//./core/hw/sh4/sh4_sched.o
extern u64 sh4_sched_ffb;
extern u32 sh4_sched_intr;
extern vector<sched_list> list;
extern int sh4_sched_next_id;




//./core/hw/sh4/interpr/sh4_interpreter.o
extern int aica_sched;
extern int rtc_sched;




//./core/hw/sh4/modules/serial.o
extern SCIF_SCFSR2_type SCIF_SCFSR2;
extern u8 SCIF_SCFRDR2;
extern SCIF_SCFDR2_type SCIF_SCFDR2;




//./core/hw/sh4/modules/bsc.o
extern BSC_PDTRA_type BSC_PDTRA;




//./core/hw/sh4/modules/tmu.o
extern u32 tmu_shift[3];
extern u32 tmu_mask[3];
extern u64 tmu_mask64[3];
extern u32 old_mode[3];
extern int tmu_sched[3];
extern u32 tmu_ch_base[3];
extern u64 tmu_ch_base64[3];




//./core/hw/sh4/modules/ccn.o
extern u32 CCN_QACR_TR[2];




//./core/hw/sh4/modules/mmu.o
extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
#if defined(NO_MMU)
extern u32 sq_remap[64];
#else
extern u32 ITLB_LRU_USE[64];
extern u32 mmu_error_TT;
#endif



//./core/imgread/common.o
extern u32 NullDriveDiscType;
//maybe - seems to all be one-time inits ;    needs special handler
//extern Disc* disc;
extern u8 q_subchannel[96];




//./core/nullDC.o
extern unsigned FLASH_SIZE;
extern unsigned BBSRAM_SIZE;
extern unsigned BIOS_SIZE;
extern unsigned RAM_SIZE;
extern unsigned ARAM_SIZE;
extern unsigned VRAM_SIZE;
extern unsigned RAM_MASK;
extern unsigned ARAM_MASK;
extern unsigned VRAM_MASK;
//settings can be dynamic
//extern settings_t settings;




//./core/reios/reios.o
//never used
//extern u8* biosrom;
//one time init
//extern u8* flashrom;
//one time init
//extern u32 base_fad ;
//one time init
//extern bool descrambl;
//one time init
//extern bool bootfile_inited;
//all these reios_?? are one-time inits
//extern char reios_bootfile[32];
//extern char reios_hardware_id[17];
//extern char reios_maker_id[17];
//extern char reios_device_info[17];
//extern char reios_area_symbols[9];
//extern char reios_peripherals[9];
//extern char reios_product_number[11];
//extern char reios_product_version[7];
//extern char reios_releasedate[17];
//extern char reios_boot_filename[17];
//extern char reios_software_company[17];
//extern char reios_software_name[129];
//one time init
//extern map<u32, hook_fp*> hooks;
//one time init
//extern map<hook_fp*, u32> hooks_rev;




//./core/reios/gdrom_hle.o
//never used in any meaningful way
//extern u32 SecMode[4];




//./core/reios/descrambl.o
//random seeds can be...random
//extern unsigned int seed;



//./core/rend/gles/gles.o
//maybe
//extern GLCache glcache;
//maybe
//extern gl_ctx gl;
//maybe
//extern struct ShaderUniforms_t ShaderUniforms;
//maybe
//extern u32 gcflip;
//maybe
//extern float fb_scale_x;
//extern float fb_scale_y;
//extern float scale_x;
//extern float scale_y;
//extern int screen_width;
//extern int screen_height;
//extern GLuint fogTextureId;
//end maybe



//./core/rend/gles/gldraw.o
//maybe
//extern PipelineShader* CurrentShader;
//written, but never used
//extern Vertex* vtx_sort_base;
//maybe
//extern vector<SortTrigDrawParam>	pidx_sort;




//./core/rend/gles/gltex.o
//maybe ; special handler
//extern map<u64,TextureCacheData> TexCache;
//maybe
//extern FBT fb_rtt;
//not used
//static int TexCacheLookups;
//static int TexCacheHits;
//static float LastTexCacheStats;
//maybe should get reset naturally if needed
//GLuint fbTextureId;




//./core/hw/naomi/naomi.o
extern u32 naomi_updates;
extern u32 RomPioOffset;
extern u32 DmaOffset;
extern u32 DmaCount;
extern u32 BoardID;
extern u32 GSerialBuffer;
extern u32 BSerialBuffer;
extern int GBufPos;
extern int BBufPos;
extern int GState;
extern int BState;
extern int GOldClk;
extern int BOldClk;
extern int BControl;
extern int BCmd;
extern int BLastCmd;
extern int GControl;
extern int GCmd;
extern int GLastCmd;
extern int SerStep;
extern int SerStep2;
extern unsigned char BSerial[];
extern unsigned char GSerial[];
extern u32 reg_dimm_3c;	//IO window ! writen, 0x1E03 some flag ?
extern u32 reg_dimm_40;	//parameters
extern u32 reg_dimm_44;	//parameters
extern u32 reg_dimm_48;	//parameters
extern u32 reg_dimm_4c;	//status/control reg ?
extern bool NaomiDataRead;
extern u32 NAOMI_ROM_OFFSETH;
extern u32 NAOMI_ROM_OFFSETL;
extern u32 NAOMI_ROM_DATA;
extern u32 NAOMI_DMA_OFFSETH;
extern u32 NAOMI_DMA_OFFSETL;
extern u32 NAOMI_DMA_COUNT;
extern u32 NAOMI_BOARDID_WRITE;
extern u32 NAOMI_BOARDID_READ;
extern u32 NAOMI_COMM_OFFSET;
extern u32 NAOMI_COMM_DATA;




//./core/hw/naomi/naomi_cart.o
//all one-time loads
//u8* RomPtr;
//u32 RomSize;
//fd_t*	RomCacheMap;
//u32		RomCacheMapCount;




//./core/rec-x64/rec_x64.o
//maybe need special handler
//extern BlockCompilerx64 *compilerx64_data;




//./core/rec.o
extern int cycle_counter;
extern int idxnxx;




//./core/hw/sh4/dyna/decoder.o
//temp storage only
//extern RuntimeBlockInfo* blk;
extern state_t state;
extern Sh4RegType div_som_reg1;
extern Sh4RegType div_som_reg2;
extern Sh4RegType div_som_reg3;




//./core/hw/sh4/dyna/driver.o
extern u8 SH4_TCB[CODE_SIZE+4096];
//one time ptr set
//extern u8* CodeCache;
extern u32 LastAddr;
extern u32 LastAddr_min;
//temp storage only
//extern u32* emit_ptr;
extern char block_hash[1024];




//./core/hw/sh4/dyna/blockmanager.o
//cleared but never read
//extern bm_List blocks_page[/*BLOCKS_IN_PAGE_LIST_COUNT*/(32*1024*1024)/4096];
//maybe - the next three seem to be list of precompiled blocks of code - but if not found will populate
//extern bm_List all_blocks;
//extern bm_List del_blocks;
//extern blkmap_t blkmap;
//these two are never referenced
//extern u32 bm_gc_luc;
//extern u32 bm_gcf_luc;
//data is never written to this
//extern u32 PAGE_STATE[(32*1024*1024)/*RAM_SIZE*//32];
//never read
//extern u32 total_saved;
//counter with no real controlling logic behind it
//extern u32 rebuild_counter;
//just printf output
//extern bool print_stats;




//./core/hw/sh4/dyna/shil.o
extern u32 RegisterWrite[sh4_reg_count];
extern u32 RegisterRead[sh4_reg_count];
extern u32 fallback_blocks;
extern u32 total_blocks;
extern u32 REMOVED_OPS;




//./core/libretro/libretro.o
extern u16 kcode[4];
extern u8 rt[4];
extern u8 lt[4];
extern u32 vks[4];
extern s8 joyx[4];
extern s8 joyy[4];



//./core/libretro-common/glsm/glsm.o
//one time setup
//extern GLint glsm_max_textures;
//one time setup
//extern struct retro_hw_render_callback hw_render;
//maybe
//extern struct gl_cached_state gl_state;


bool serialize(void *src, unsigned int src_size, void **dest, unsigned int *total_size)
{
	if ( *dest != NULL )
	{
		memcpy(*dest, src, src_size) ;
		*dest = ((unsigned char*)*dest) + src_size ;
	}

	*total_size += src_size ;
	return true ;
}

bool unserialize(void *src, unsigned int src_size, void **dest, unsigned int *total_size)
{
	if ( *dest != NULL )
	{
		memcpy(src, *dest, src_size) ;
		*dest = ((unsigned char*)*dest) + src_size ;
	}

	*total_size += src_size ;
	return true ;
}

bool register_serialize(Array<RegisterStruct>& regs,void **data, unsigned int *total_size )
{
	int i = 0 ;

	for ( i = 0 ; i < regs.Size ; i++ )
	{
		s(regs.data[i].flags) ;
		s(regs.data[i].data32) ;
	}

	return true ;
}

bool register_unserialize(Array<RegisterStruct>& regs,void **data, unsigned int *total_size )
{
	int i = 0 ;
	u32 dummy = 0 ;

	for ( i = 0 ; i < regs.Size ; i++ )
	{
		us(regs.data[i].flags) ;
		if ( ! (regs.data[i].flags & REG_RF) )
			us(regs.data[i].data32) ;
		else
			us(dummy) ;
	}
	return true ;
}

bool dc_serialize(void **data, unsigned int *total_size)
{
	int i = 0;
	int j = 0;
	serialize_version_enum version = V1 ;

	*total_size = 0 ;

	//dc not initialized yet
	if ( p_sh4rcb == NULL )
		return false ;

	s(version) ;
	s(aica_interr) ;
	s(aica_reg_L) ;
	s(e68k_out) ;
	s(e68k_reg_L) ;
	s(e68k_reg_M) ;

	sa(arm_Reg,RN_ARM_REG_COUNT);
	s(armIrqEnable);
	s(armFiqEnable);
	s(armMode);
	s(Arm7Enabled);
	sa(cpuBitsSet,256);
	s(intState);
	s(stopState);
	s(holdState);

	s(dsp);

	for ( i = 0 ; i < 3 ; i++)
	{
		s(timers[i].c_step);
		s(timers[i].m_step);
	}


	sa(aica_ram.data,aica_ram.size) ;
	s(VREG);
	s(ARMRST);
	s(rtc_EN);
	s(dma_sched_id);

	sa(aica_reg,0x8000);



	sa(volume_lut,16);
	sa(tl_lut,256 + 768);
	sa(AEG_ATT_SPS,64);
	sa(AEG_DSR_SPS,64);
	s(pl);
	s(pr);

	channel_serialize(data, total_size) ;

	sa(cdda_sector,CDDA_SIZE);
	s(cdda_index);
	sa(mxlr,64);
	s(samples_gen);


	register_serialize(sb_regs, data, total_size) ;
	s(SB_ISTNRM);
	s(SB_FFST_rc);
	s(SB_FFST);



	//this is one-time init, no updates - don't need to serialize
	//extern RomChip sys_rom;

	s(sys_nvmem_sram.size);
	s(sys_nvmem_sram.mask);
	sa(sys_nvmem_sram.data,sys_nvmem_sram.size);

	s(sys_nvmem_flash.size);
	s(sys_nvmem_flash.mask);
	s(sys_nvmem_flash.state);
	sa(sys_nvmem_flash.data,sys_nvmem_flash.size);

	//this is one-time init, no updates - don't need to serialize
	//extern _vmem_handler area0_handler;




	sa(reply_11,16) ;



	s(gdrom_sched);
	s(sns_asc);
	s(sns_ascq);
	s(sns_key);

	s(packet_cmd);
	s(set_mode_offset);
	s(read_params);
	s(packet_cmd);
	s(read_buff);
	s(pio_buff);
	s(set_mode_offset);
	s(ata_cmd);
	s(cdda);
	s(gd_state);
	s(gd_disk_type);
	s(data_write_mode);
	s(DriveSel);
	s(Error);
	s(IntReason);
	s(Features);
	s(SecCount);
	s(SecNumber);
	s(GDStatus);
	s(ByteCount);
	s(GDROM_TICK);


	sa(EEPROM,0x100);
	s(EEPROM_loaded);
	s(State);


	s(maple_ddt_pending_reset);

	for (i = 0 ; i < 4 ; i++)
		for (j = 0 ; j < 6 ; j++)
			if ( MapleDevices[i][j] != 0 )
				MapleDevices[i][j]->maple_serialize(data, total_size) ;



	s(FrameCount);
	s(pend_rend);


	sa(YUV_tempdata,512/4);
	s(YUV_dest);
	s(YUV_blockcount);
	s(YUV_x_curr);
	s(YUV_y_curr);
	s(YUV_x_size);
	s(YUV_y_size);




	s(fog_needs_update);
	sa(pvr_regs,pvr_RegSize);




	s(in_vblank);
	s(clc_pvr_scanline);
	s(pvr_numscanlines);
	s(prv_cur_scanline);
	s(vblk_cnt);
	s(Line_Cycles);
	s(Frame_Cycles);
	s(render_end_sched);
	s(vblank_sched);
	s(time_sync);
	s(speed_load_mspdf);
	s(mips_counter);
	s(full_rps);



	sa(ta_type_lut,256);
	sa(ta_fsm,2049);
	s(ta_fsm_cl);


	s(pal_needs_update);
	sa(_pal_rev_256,4);
	sa(_pal_rev_16,64);
	sa(pal_rev_256,4);
	sa(pal_rev_16,64);
	for ( i = 0 ; i < 3 ; i++ )
	{
		u32 *ptr = decoded_colors[i] ;
		sa(ptr,65536);
	}
	s(tileclip_val);
	sa(f32_su8_tbl,65536);
	sa(FaceBaseColor,4);
	sa(FaceOffsColor,4);
	s(SFaceBaseColor);
	s(SFaceOffsColor);


	s(palette_index);
	s(KillTex);
	sa(palette16_ram,1024);
	sa(palette32_ram,1024);
	for (i = 0 ; i < 2 ; i++)
		for (j = 0 ; j < 8 ; j++)
		{
			u32 *ptr = detwiddle[i][j] ;
			sa(ptr,1024);
		}
	sa(vram.data, vram.size);




	sa(OnChipRAM.data,OnChipRAM_SIZE);

	register_serialize(CCN, data, total_size) ;
	register_serialize(UBC, data, total_size) ;
	register_serialize(BSC, data, total_size) ;
	register_serialize(DMAC, data, total_size) ;
	register_serialize(CPG, data, total_size) ;
	register_serialize(RTC, data, total_size) ;
	register_serialize(INTC, data, total_size) ;
	register_serialize(TMU, data, total_size) ;
	register_serialize(SCI, data, total_size) ;
	register_serialize(SCIF, data, total_size) ;

	sa(mem_b.data, mem_b.size);



	s(IRLPriority);
	sa(InterruptEnvId,32);
	sa(InterruptBit,32);
	sa(InterruptLevelBit,16);
	s(interrupt_vpend);
	s(interrupt_vmask);
	s(decoded_srimask);




	//default to nommu_full
	i = 3 ;
	if ( do_sqw_nommu == &do_sqw_nommu_area_3)
		i = 0 ;
	else if (do_sqw_nommu == &do_sqw_nommu_area_3_nonvmem)
		i = 1 ;
	else if (do_sqw_nommu==(sqw_fp*)&TAWriteSQ)
		i = 2 ;
	else if (do_sqw_nommu==&do_sqw_nommu_full)
		i = 3 ;

	s(i) ;

	sa((*p_sh4rcb).sq_buffer,64/8);

	//store these before unserializing and then restore after
	//void *getptr = &((*p_sh4rcb).cntx.sr.GetFull) ;
	//void *setptr = &((*p_sh4rcb).cntx.sr.SetFull) ;
	s((*p_sh4rcb).cntx);

	s(old_rm);
	s(old_dn);




	s(sh4_sched_ffb);
	s(sh4_sched_intr);
	s(sh4_sched_next_id);
	//this list is populated during initialization so the size will always be the same
	//extern vector<sched_list> list;
	for ( i = 0 ; i < list.size() ; i++ )
	{
		s(list[i].tag) ;
		s(list[i].start) ;
		s(list[i].end) ;
	}



	s(aica_sched);
	s(rtc_sched);


	s(SCIF_SCFSR2);
	s(SCIF_SCFRDR2);
	s(SCIF_SCFDR2);


	s(BSC_PDTRA);




	sa(tmu_shift,3);
	sa(tmu_mask,3);
	sa(tmu_mask64,3);
	sa(old_mode,3);
	sa(tmu_sched,3);
	sa(tmu_ch_base,3);
	sa(tmu_ch_base64,3);




	sa(CCN_QACR_TR,2);




	sa(UTLB,64);
	sa(ITLB,4);
#if defined(NO_MMU)
	sa(sq_remap,64);
#else
	sa(ITLB_LRU_USE,64);
	s(mmu_error_TT);
#endif



	s(NullDriveDiscType);
	sa(q_subchannel,96);


	s(FLASH_SIZE);
	s(BBSRAM_SIZE);
	s(BIOS_SIZE);
	s(RAM_SIZE);
	s(ARAM_SIZE);
	s(VRAM_SIZE);
	s(RAM_MASK);
	s(ARAM_MASK);
	s(VRAM_MASK);



	s(naomi_updates);
	s(RomPioOffset);
	s(DmaOffset);
	s(DmaCount);
	s(BoardID);
	s(GSerialBuffer);
	s(BSerialBuffer);
	s(GBufPos);
	s(BBufPos);
	s(GState);
	s(BState);
	s(GOldClk);
	s(BOldClk);
	s(BControl);
	s(BCmd);
	s(BLastCmd);
	s(GControl);
	s(GCmd);
	s(GLastCmd);
	s(SerStep);
	s(SerStep2);
	sa(BSerial,69);
	sa(GSerial,69);
	s(reg_dimm_3c);
	s(reg_dimm_40);
	s(reg_dimm_44);
	s(reg_dimm_48);
	s(reg_dimm_4c);
	s(NaomiDataRead);
	s(NAOMI_ROM_OFFSETH);
	s(NAOMI_ROM_OFFSETL);
	s(NAOMI_ROM_DATA);
	s(NAOMI_DMA_OFFSETH);
	s(NAOMI_DMA_OFFSETL);
	s(NAOMI_DMA_COUNT);
	s(NAOMI_BOARDID_WRITE);
	s(NAOMI_BOARDID_READ);
	s(NAOMI_COMM_OFFSET);
	s(NAOMI_COMM_DATA);

	s(cycle_counter);
	s(idxnxx);


	s(state);
	s(div_som_reg1);
	s(div_som_reg2);
	s(div_som_reg3);




	//sa(CodeCache,CODE_SIZE) ;
	//sa(SH4_TCB,CODE_SIZE+4096);
	s(LastAddr);
	s(LastAddr_min);
	sa(block_hash,1024);


	sa(RegisterWrite,sh4_reg_count);
	sa(RegisterRead,sh4_reg_count);
	s(fallback_blocks);
	s(total_blocks);
	s(REMOVED_OPS);




	sa(kcode,4);
	sa(rt,4);
	sa(lt,4);
	sa(vks,4);
	sa(joyx,4);
	sa(joyy,4);



	return true ;
}

bool dc_unserialize(void **data, unsigned int *total_size)
{
	int i = 0;
	int j = 0;
	serialize_version_enum version = V1 ;

	*total_size = 0 ;

	bm_Reset() ;

	us(version) ;
	us(aica_interr) ;
	us(aica_reg_L) ;
	us(e68k_out) ;
	us(e68k_reg_L) ;
	us(e68k_reg_M) ;

	usa(arm_Reg,RN_ARM_REG_COUNT);
	us(armIrqEnable);
	us(armFiqEnable);
	us(armMode);
	us(Arm7Enabled);
	usa(cpuBitsSet,256);
	us(intState);
	us(stopState);
	us(holdState);

	us(dsp);

	for ( i = 0 ; i < 3 ; i++)
	{
		us(timers[i].c_step);
		us(timers[i].m_step);
	}


	usa(aica_ram.data,aica_ram.size) ;
	us(VREG);
	us(ARMRST);
	us(rtc_EN);
	us(dma_sched_id);

	usa(aica_reg,0x8000);



	usa(volume_lut,16);
	usa(tl_lut,256 + 768);
	usa(AEG_ATT_SPS,64);
	usa(AEG_DSR_SPS,64);
	us(pl);
	us(pr);

	channel_unserialize(data, total_size) ;

	usa(cdda_sector,CDDA_SIZE);
	us(cdda_index);
	usa(mxlr,64);
	us(samples_gen);


	register_unserialize(sb_regs, data, total_size) ;
	us(SB_ISTNRM);
	us(SB_FFST_rc);
	us(SB_FFST);



	//this is one-time init, no updates - don't need to serialize
	//extern RomChip sys_rom;

	us(sys_nvmem_sram.size);
	us(sys_nvmem_sram.mask);
	usa(sys_nvmem_sram.data,sys_nvmem_sram.size);

	us(sys_nvmem_flash.size);
	us(sys_nvmem_flash.mask);
	us(sys_nvmem_flash.state);
	usa(sys_nvmem_flash.data,sys_nvmem_flash.size);

	//this is one-time init, no updates - don't need to serialize
	//extern _vmem_handler area0_handler;




	usa(reply_11,16) ;



	us(gdrom_sched);
	us(sns_asc);
	us(sns_ascq);
	us(sns_key);

	us(packet_cmd);
	us(set_mode_offset);
	us(read_params);
	us(packet_cmd);
	us(read_buff);
	us(pio_buff);
	us(set_mode_offset);
	us(ata_cmd);
	us(cdda);
	us(gd_state);
	us(gd_disk_type);
	us(data_write_mode);
	us(DriveSel);
	us(Error);
	us(IntReason);
	us(Features);
	us(SecCount);
	us(SecNumber);
	us(GDStatus);
	us(ByteCount);
	us(GDROM_TICK);


	usa(EEPROM,0x100);
	us(EEPROM_loaded);
	us(State);


	us(maple_ddt_pending_reset);

	for (i = 0 ; i < 4 ; i++)
		for (j = 0 ; j < 6 ; j++)
			if ( MapleDevices[i][j] != 0 )
				MapleDevices[i][j]->maple_unserialize(data, total_size) ;



	us(FrameCount);
	us(pend_rend);


	usa(YUV_tempdata,512/4);
	us(YUV_dest);
	us(YUV_blockcount);
	us(YUV_x_curr);
	us(YUV_y_curr);
	us(YUV_x_size);
	us(YUV_y_size);




	us(fog_needs_update);
	usa(pvr_regs,pvr_RegSize);




	us(in_vblank);
	us(clc_pvr_scanline);
	us(pvr_numscanlines);
	us(prv_cur_scanline);
	us(vblk_cnt);
	us(Line_Cycles);
	us(Frame_Cycles);
	us(render_end_sched);
	us(vblank_sched);
	us(time_sync);
	us(speed_load_mspdf);
	us(mips_counter);
	us(full_rps);



	usa(ta_type_lut,256);
	usa(ta_fsm,2049);
	us(ta_fsm_cl);


	us(pal_needs_update);
	usa(_pal_rev_256,4);
	usa(_pal_rev_16,64);
	usa(pal_rev_256,4);
	usa(pal_rev_16,64);
	for ( i = 0 ; i < 3 ; i++ )
	{
		u32 *ptr = decoded_colors[i] ;
		usa(ptr,65536);
	}
	us(tileclip_val);
	usa(f32_su8_tbl,65536);
	usa(FaceBaseColor,4);
	usa(FaceOffsColor,4);
	us(SFaceBaseColor);
	us(SFaceOffsColor);


	us(palette_index);
	us(KillTex);
	usa(palette16_ram,1024);
	usa(palette32_ram,1024);
	for (i = 0 ; i < 2 ; i++)
		for (j = 0 ; j < 8 ; j++)
		{
			u32 *ptr = detwiddle[i][j] ;
			usa(ptr,1024);
		}
	usa(vram.data, vram.size);




	usa(OnChipRAM.data,OnChipRAM_SIZE);

	register_unserialize(CCN, data, total_size) ;
	register_unserialize(UBC, data, total_size) ;
	register_unserialize(BSC, data, total_size) ;
	register_unserialize(DMAC, data, total_size) ;
	register_unserialize(CPG, data, total_size) ;
	register_unserialize(RTC, data, total_size) ;
	register_unserialize(INTC, data, total_size) ;
	register_unserialize(TMU, data, total_size) ;
	register_unserialize(SCI, data, total_size) ;
	register_unserialize(SCIF, data, total_size) ;

	usa(mem_b.data, mem_b.size);



	us(IRLPriority);
	usa(InterruptEnvId,32);
	usa(InterruptBit,32);
	usa(InterruptLevelBit,16);
	us(interrupt_vpend);
	us(interrupt_vmask);
	us(decoded_srimask);




	us(i) ;
	if ( i == 0 )
		do_sqw_nommu = &do_sqw_nommu_area_3 ;
	else if ( i == 1 )
		do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem ;
	else if ( i == 2 )
		do_sqw_nommu = (sqw_fp*)&TAWriteSQ ;
	else if ( i == 3 )
		do_sqw_nommu = &do_sqw_nommu_full ;



	usa((*p_sh4rcb).sq_buffer,64/8);

	//store these before unserializing and then restore after
	//void *getptr = &((*p_sh4rcb).cntx.sr.GetFull) ;
	//void *setptr = &((*p_sh4rcb).cntx.sr.SetFull) ;
	us((*p_sh4rcb).cntx);
	//(*p_sh4rcb).cntx.sr.GetFull = getptr ;
	//(*p_sh4rcb).cntx.sr.SetFull = setptr ;

	us(old_rm);
	us(old_dn);




	us(sh4_sched_ffb);
	us(sh4_sched_intr);
	us(sh4_sched_next_id);
	//this list is populated during initialization so the size will always be the same
	//extern vector<sched_list> list;
	for ( i = 0 ; i < list.size() ; i++ )
	{
		us(list[i].tag) ;
		us(list[i].start) ;
		us(list[i].end) ;
	}



	us(aica_sched);
	us(rtc_sched);


	us(SCIF_SCFSR2);
	us(SCIF_SCFRDR2);
	us(SCIF_SCFDR2);


	us(BSC_PDTRA);




	usa(tmu_shift,3);
	usa(tmu_mask,3);
	usa(tmu_mask64,3);
	usa(old_mode,3);
	usa(tmu_sched,3);
	usa(tmu_ch_base,3);
	usa(tmu_ch_base64,3);




	usa(CCN_QACR_TR,2);




	usa(UTLB,64);
	usa(ITLB,4);
#if defined(NO_MMU)
	usa(sq_remap,64);
#else
	usa(ITLB_LRU_USE,64);
	us(mmu_error_TT);
#endif




	us(NullDriveDiscType);
	usa(q_subchannel,96);


	us(FLASH_SIZE);
	us(BBSRAM_SIZE);
	us(BIOS_SIZE);
	us(RAM_SIZE);
	us(ARAM_SIZE);
	us(VRAM_SIZE);
	us(RAM_MASK);
	us(ARAM_MASK);
	us(VRAM_MASK);



	us(naomi_updates);
	us(RomPioOffset);
	us(DmaOffset);
	us(DmaCount);
	us(BoardID);
	us(GSerialBuffer);
	us(BSerialBuffer);
	us(GBufPos);
	us(BBufPos);
	us(GState);
	us(BState);
	us(GOldClk);
	us(BOldClk);
	us(BControl);
	us(BCmd);
	us(BLastCmd);
	us(GControl);
	us(GCmd);
	us(GLastCmd);
	us(SerStep);
	us(SerStep2);
	usa(BSerial,69);
	usa(GSerial,69);
	us(reg_dimm_3c);
	us(reg_dimm_40);
	us(reg_dimm_44);
	us(reg_dimm_48);
	us(reg_dimm_4c);
	us(NaomiDataRead);
	us(NAOMI_ROM_OFFSETH);
	us(NAOMI_ROM_OFFSETL);
	us(NAOMI_ROM_DATA);
	us(NAOMI_DMA_OFFSETH);
	us(NAOMI_DMA_OFFSETL);
	us(NAOMI_DMA_COUNT);
	us(NAOMI_BOARDID_WRITE);
	us(NAOMI_BOARDID_READ);
	us(NAOMI_COMM_OFFSET);
	us(NAOMI_COMM_DATA);

	us(cycle_counter);
	us(idxnxx);


	us(state);
	us(div_som_reg1);
	us(div_som_reg2);
	us(div_som_reg3);




	//usa(CodeCache,CODE_SIZE) ;
	//usa(SH4_TCB,CODE_SIZE+4096);
	us(LastAddr);
	us(LastAddr_min);
	usa(block_hash,1024);


	usa(RegisterWrite,sh4_reg_count);
	usa(RegisterRead,sh4_reg_count);
	us(fallback_blocks);
	us(total_blocks);
	us(REMOVED_OPS);




	usa(kcode,4);
	usa(rt,4);
	usa(lt,4);
	usa(vks,4);
	usa(joyx,4);
	usa(joyy,4);

	return true ;
}
