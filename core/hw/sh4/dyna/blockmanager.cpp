/*
	Tiny cute block manager. Doesn't keep block graphs or anything fancy ...
	Its based on a simple hashed-lists idea
*/

#include <algorithm>
#include <set>
#include <map>

#include "blockmanager.h"
#include "ngen.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_if.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/aica/aica_if.h"
//#include "../dmac.h"
#include "hw/gdrom/gdrom_if.h"
//#include "../intc.h"
//#include "../tmu.h"
#include "hw/sh4/sh4_mem.h"


#if FEAT_SHREC != DYNAREC_NONE


typedef set<RuntimeBlockInfo*> bm_List;

bm_List all_blocks;
bm_List del_blocks;

std::map<void*, RuntimeBlockInfo*> blkmap;
u32 bm_gc_luc,bm_gcf_luc;


#define FPCA(x) ((DynarecCodeEntryPtr&)sh4rcb.fpcb[(x>>1)&FPCB_MASK])

// This returns an executable address
DynarecCodeEntryPtr DYNACALL bm_GetCode(u32 addr)
{
	DynarecCodeEntryPtr rv = (DynarecCodeEntryPtr)FPCA(addr);

	return (DynarecCodeEntryPtr)rv;
}

// This returns an executable address
DynarecCodeEntryPtr DYNACALL bm_GetCode2(u32 addr)
{
	return (DynarecCodeEntryPtr)bm_GetCode(addr);
}

// This returns an executable address
RuntimeBlockInfo* DYNACALL bm_GetBlock(u32 addr)
{
	DynarecCodeEntryPtr cde = bm_GetCode(addr);  // Returns RX ptr

	if (cde == ngen_FailedToFindBlock)
		return 0;
	else
		return bm_GetBlock((void*)cde);  // Returns RX pointer
}

// This takes a RX address and returns the info block ptr (RW space)
RuntimeBlockInfo* bm_GetBlock(void* dynarec_code)
{
	if (blkmap.empty())
		return 0;

	void *dynarecrw = CC_RX2RW(dynarec_code);
	// Returns a block who's code addr is bigger than dynarec_code (or end)
	auto iter = blkmap.upper_bound(dynarecrw);
	iter--;  // Need to go back to find the potential candidate

	// However it might be out of bounds, check for that
	if ((char*)iter->second->code + iter->second->host_code_size < dynarec_code)
		return 0;

	verify(iter->second->contains_code((u8*)dynarecrw));
	return iter->second;
}

void bm_CleanupDeletedBlocks()
{
	for (auto it = del_blocks.begin(); it != del_blocks.end(); it++)
		delete *it;

	del_blocks.clear();
}

// Takes RX pointer and returns a RW pointer
RuntimeBlockInfo* bm_GetStaleBlock(void* dynarec_code)
{
	void *dynarecrw = CC_RX2RW(dynarec_code);
	
	bm_CleanupDeletedBlocks();

	return 0;
}

void bm_AddBlock(RuntimeBlockInfo* blk)
{
	auto iter = blkmap.find((void*)blk->code);
	if (iter != blkmap.end()) {
		printf("DUP: %08X %p %08X %p\n", iter->second->addr, iter->second->code, blk->addr, blk->code);
		die("bm_AddBlock: dupplicate");
	}
	blkmap[(void*)blk->code] = blk;
	all_blocks.insert(blk);

	verify((void*)bm_GetCode(blk->addr)==(void*)ngen_FailedToFindBlock);
	FPCA(blk->addr) = (DynarecCodeEntryPtr)CC_RW2RX(blk->code);
}

void bm_DiscardBlock(RuntimeBlockInfo* blk)
{
	auto iter = blkmap.find((void*)blk->code);
	if (iter == blkmap.end()) {
		printf("Missing: %p\n", blk->code);
		die("bm_DiscardBlock: missing");
	}

	blkmap.erase((void*)blk->code);
	all_blocks.erase(blk);
	
	blk->Discard();

	del_blocks.insert(blk);

	verify((void*)bm_GetCode(blk->addr)==(void*)blk->code);
	FPCA(blk->addr) = (DynarecCodeEntryPtr)&ngen_FailedToFindBlock;
	verify((void*)bm_GetCode(blk->addr)==(void*)ngen_FailedToFindBlock);
}

void bm_DiscardAddress(u32 codeaddr)
{
	for (auto it=all_blocks.begin(); it!=all_blocks.end(); it++)
	{
		if ( ((*it)->addr <= codeaddr) && ((*it)->addr + (*it)->sh4_code_size) > codeaddr )
		{
			bm_DiscardBlock(*it);
		}
	}
}

void bm_Periodical_1s()
{
	bm_CleanupDeletedBlocks();
}

void bm_vmem_pagefill(void** ptr, u32 size_bytes)
{
	for (size_t i=0; i < size_bytes / sizeof(ptr[0]); i++)
	{
		ptr[i]=(void*)ngen_FailedToFindBlock;
	}
}

void bm_Reset()
{
	ngen_ResetBlocks();
	_vmem_bm_reset();

	for (auto it=all_blocks.begin(); it!=all_blocks.end(); it++)
	{
		(*it)->relink_data=0;
		(*it)->pNextBlock=0;
		(*it)->pBranchBlock=0;
		(*it)->Relink();
	}

	del_blocks.insert(all_blocks.begin(),all_blocks.end());

	all_blocks.clear();
	blkmap.clear();
}


void bm_Init()
{

}

void bm_Term()
{
	bm_Reset();
	bm_CleanupDeletedBlocks();
}

RuntimeBlockInfo::~RuntimeBlockInfo()
{

}

void RuntimeBlockInfo::AddRef(RuntimeBlockInfo* other) 
{ 
	pre_refs.push_back(other); 
}

void RuntimeBlockInfo::RemRef(RuntimeBlockInfo* other) 
{ 
	pre_refs.erase(find(pre_refs.begin(),pre_refs.end(),other)); 
}

void RuntimeBlockInfo::Discard()
{
	die("Discard not implemented");
}

bool print_stats;

void fprint_hex(FILE* d,const char* init,u8* ptr, u32& ofs, u32 limit)
{
	int base=ofs;
	int cnt=0;
	while(ofs<limit)
	{
		if (cnt==32)
		{
			fputs("\n",d);
			cnt=0;
		}

		if (cnt==0)
			fprintf(d,"%s:%d:",init,ofs-base);

		fprintf(d," %02X",ptr[ofs++]);
		cnt++;
	}
	fputs("\n",d);
}

void bm_WriteBlockMap(const string& file)
{
	die("bm_WriteBlockMap is noop");
}

void bm_sh4_jitsym(FILE* out)
{
	die("bm_sh4_jitsym is noop");
}

#endif

