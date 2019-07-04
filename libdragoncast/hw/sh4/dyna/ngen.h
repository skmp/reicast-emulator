/*
	Header file for native generator interface
	Needs some cleanup


	SH4 -> Code gen -> Ram

	Ram -> link/relocate -> Staging buffer
	Ram -> link/relocate -> Steady-state buffer

	Staging      : scratch, relatively small, circular code buffer
	Steady state : 'final' code buffer. When blocks reach a steady-state, they get copied here

	When the Staging buffer is full, a reset is done on the dynarec.
	If the stating buffer is full, but re-locating everything will free enough space, it will be relocated (GC'd)

	If the stating buffer is full, then blocks in it are put into "hibernation"

	Block can be
	in Ram, only ('hibernated')
	in Ram + steady state buffer
	in Ram + staging buffer

	Changes required on the ngen/dynarecs for this to work

	- Support relocation
	- Support re-linking
	- Support hibernated blocks, or block removal

	Changes on BM

	- Block graph
	- Block removal
	- Relocation driving logic


	This will enable

	- Extensive block specialisation (Further mem opts, other things that might gain)
	- Possibility of superblock chains
*/

#pragma once
#include "rec_config.h"
#include "decoder.h"
#include "blockmanager.h"


#define CODE_SIZE   (10*1024*1024)

// When NO_RWX is enabled there's two address-spaces, one executable and
// one writtable. The emitter and most of the code in rec-* will work with
// the RW pointer. However the fpcb table and other pointers during execution
// (ie. exceptions) are RX pointers. These two macros convert between them by
// sub/add the pointer offset. CodeCache will point to the RW pointer for simplicity.
#ifdef FEAT_NO_RWX_PAGES
	extern uintptr_t cc_rx_offset;
	#define CC_RW2RX(ptr) (void*)(((uintptr_t)(ptr)) + cc_rx_offset)
	#define CC_RX2RW(ptr) (void*)(((uintptr_t)(ptr)) - cc_rx_offset)
#else
	#define CC_RW2RX(ptr) (ptr)
	#define CC_RX2RW(ptr) (ptr)
#endif

//alternative emit ptr, set to 0 to use the main buffer
extern u32* emit_ptr;
extern u8* CodeCache;

#ifdef __cplusplus
extern "C" {
#endif

void emit_Write32(u32 data);
void emit_Skip(u32 sz);
u32 emit_FreeSpace();
void* emit_GetCCPtr();
void emit_SetBaseAddr();

//Called from ngen_FailedToFindBlock
DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc);
//Called when a block check failed, and the block needs to be invalidated
DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 pc);
//Called to compile code @pc
DynarecCodeEntryPtr rdv_CompilePC();
//Returns 0 if there is no code @pc, code ptr otherwise
DynarecCodeEntryPtr rdv_FindCode();
//Finds or compiles code @pc
DynarecCodeEntryPtr rdv_FindOrCompile();

//code -> pointer to code of block, dpc -> if dynamic block, pc. if cond, 0 for next, 1 for branch
void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc);

u32 DYNACALL rdv_DoInterrupts(void* block_cpde);
u32 DYNACALL rdv_DoInterrupts_pc(u32 pc);

#ifdef __cplusplus
}
#endif


//Stuff to be implemented per dynarec core

typedef void (*MainloopFnPtr_t) (void*);

//Canonical callback interface
enum CanonicalParamType
{
	CPT_u32,
	CPT_u32rv,
	CPT_u64rvL,
	CPT_u64rvH,
	CPT_f32,
	CPT_f32rv,
	CPT_ptr,
};

struct NGenBackend
{
	virtual bool Init() = 0;

	//Called to compile a block
	virtual void Compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise) = 0;

	//Called when blocks are reseted
	virtual void OnResetBlocks() = 0;

	virtual void GetFeatures(ngen_features* dst) = 0;

	// Canonical interface
	virtual void CC_Start(shil_opcode* op) = 0;
	virtual void CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp) = 0;
	virtual void CC_Call(shil_opcode* op, void* function) = 0;
	virtual void CC_Finish(shil_opcode* op) = 0;

	// Alloc block, null return if out of blocks
	virtual RuntimeBlockInfo* AllocateBlock() = 0;

	virtual bool Rewrite(unat& addr, unat retadr, unat acc) = 0;
	virtual u32* ReadmFail(u32* ptr, u32* regs, u32 saddr) = 0;

	// assembly exports

	MainloopFnPtr_t Mainloop;

	//Value to be returned when the block manager failed to find a block,
	//should call rdv_FailedToFindBlock and then jump to the return value
	void (*FailedToFindBlock)();
};

struct ngen_backend_t
{
	string slug;
	string name;

	NGenBackend* (*Create)();
};

bool rdv_RegisterShilBackend(const ngen_backend_t& backend);

extern NGenBackend* rdv_ngen;