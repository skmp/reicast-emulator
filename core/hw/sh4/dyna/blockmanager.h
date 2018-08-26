/*
	In case you wonder, the extern "C" stuff are for the assembly code on beagleboard/pandora
*/
#include "types.h"
#include "decoder.h"
#include <set>
#pragma once


#define CODE_SIZE   (16*1024*1024)
typedef void (*DynarecCodeEntryPtr)();

extern u8* CodeCache;

struct RuntimeBlockInfo_Core
{
	u32 addr;
	DynarecCodeEntryPtr code;
	u32 lookups;
};

struct RuntimeBlockInfo: RuntimeBlockInfo_Core
{
	void Setup(u32 pc,fpscr_t fpu_cfg);
	const char* hash(bool full=true, bool reloc=false);

	u32 host_code_size;	   /* in bytes */
	u32 sh4_code_size;      /* in bytes */

	u32 runs;
	s32 staging_runs;

	fpscr_t fpu_cfg;
	u32 guest_cycles;
	u32 guest_opcodes;
	u32 host_opcodes;


	u32 BranchBlock; /* if not 0xFFFFFFFF then jump target */
	u32 NextBlock;   /* if not 0xFFFFFFFF then next block (by position) */

	/* 0 if not available */
	RuntimeBlockInfo* pBranchBlock;
	RuntimeBlockInfo* pNextBlock; 

	u32 relink_offset;
	u32 relink_data;
	u32 csc_RetCache; /* only for stats for now */

	BlockEndType BlockType;
	bool has_jcond;

	vector<shil_opcode> oplist;

	bool contains_code(u8* ptr)
	{
		return ((size_t)(ptr-(u8*)code)) < host_code_size;
	}

	virtual ~RuntimeBlockInfo();

	virtual u32 Relink()=0;
	virtual void Relocate(void* dst)=0;
	
	/* predecessors references */
	vector<RuntimeBlockInfo*> pre_refs;

	void AddRef(RuntimeBlockInfo* other);
	void RemRef(RuntimeBlockInfo* other);

	void Discard();
	void UpdateRefs();

	u32 memops;
	u32 linkedmemops;
};

struct CachedBlockInfo: RuntimeBlockInfo_Core
{
	RuntimeBlockInfo* block;
};

#if FEAT_SHREC != DYNAREC_NONE
typedef vector<RuntimeBlockInfo*> bm_List;
#endif

struct BlockMapCMP
{
	static bool is_code(RuntimeBlockInfo* blk)
	{
		if ((unat)((u8*)blk-CodeCache)<CODE_SIZE)
			return true;
		else
			return false;
	}

	static unat get_blkstart(RuntimeBlockInfo* blk)
	{
		if (is_code(blk))
			return (unat)blk;
		else
			return (unat)blk->code;
	}

	static unat get_blkend(RuntimeBlockInfo* blk)
	{
		if (is_code(blk))
			return (unat)blk;
		else
			return (unat)blk->code+blk->host_code_size-1;
	}

	//return true if blkl > blkr
	bool operator()(RuntimeBlockInfo* blkl, RuntimeBlockInfo* blkr) const
	{
		if (!is_code(blkl) && !is_code(blkr))
			return (unat)blkl->code<(unat)blkr->code;

		unat blkr_start=get_blkstart(blkr),blkl_end=get_blkend(blkl);

		if (blkl_end<blkr_start)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};

typedef std::set<RuntimeBlockInfo*,BlockMapCMP> blkmap_t;

void bm_WriteBlockMap(const string& file);

extern "C" DynarecCodeEntryPtr DYNACALL bm_GetCode(u32 addr);

RuntimeBlockInfo* bm_GetBlock2(void* dynarec_code);
RuntimeBlockInfo* bm_GetStaleBlock(void* dynarec_code);
RuntimeBlockInfo* DYNACALL bm_GetBlock(u32 addr);

void bm_AddBlock(RuntimeBlockInfo* blk);
void bm_Reset();
void bm_Periodical_1s();
void bm_Periodical_14k();

void bm_Init();
void bm_Term();

void bm_vmem_pagefill(void** ptr,u32 PAGE_SZ);
