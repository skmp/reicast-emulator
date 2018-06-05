#include "types.h"
#include "pvr.h"
#include "spg.h"
#include "ta.h"
#include "hw/mem/_vmem.h"

#include "ta.h"
#include "tr.h"

//TODO : move code later to a plugin
//TODO : Fix registers arrays , they must be smaller now doe to the way SB registers are handled
#include "hw/holly/holly.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mem.h"

#include <rthreads/rthreads.h>

vector<vram_block*> VramLocks[VRAM_SIZE/PAGE_SIZE];
//vram 32-64b
VArray2 vram;

//List functions
//
static void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end  = block->end/PAGE_SIZE;

	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (size_t j=0;j<list->size();j++)
		{
			if ((*list)[j]==block)
				(*list)[j]=0;
		}
	}
}
 
static void vramlock_list_add(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end = block->end/PAGE_SIZE;


	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (u32 j=0;j<list->size();j++)
		{
			if ((*list)[j]==0)
			{
				(*list)[j]=block;
				goto added_it;
			}
		}

		list->push_back(block);
added_it:
		i=i;
	}
}
 
#ifndef TARGET_NO_THREADS
slock_t *vramlist_lock;
#endif

//simple IsInRange test
static inline bool IsInRange(vram_block* block,u32 offset)
{
	return (block->start<=offset) && (block->end>=offset);
}

vram_block* libCore_vramlock_Lock(u32 start_offset64,u32 end_offset64,void* userdata)
{
	vram_block* block=(vram_block* )malloc(sizeof(vram_block));

	if (end_offset64>(VRAM_SIZE-1))
	{
		msgboxf("vramlock_Lock_64: end_offset64>(VRAM_SIZE-1) \n Tried to lock area out of vram , possibly bug on the pvr plugin",MBX_OK);
		end_offset64=(VRAM_SIZE-1);
	}

	if (start_offset64>end_offset64)
	{
		msgboxf("vramlock_Lock_64: start_offset64>end_offset64 \n Tried to lock negative block , possibly bug on the pvr plugin",MBX_OK);
		start_offset64=0;
	}

	block->end=end_offset64;
	block->start=start_offset64;
	block->len=end_offset64-start_offset64+1;
	block->userdata=userdata;
	block->type=64;

#ifndef TARGET_NO_THREADS
   slock_lock(vramlist_lock);
#endif

   VArray2_LockRegion(&vram, block->start,block->len);

   //TODO: Fix this for 32M wrap as well
   if (_nvmem_enabled() && VRAM_SIZE == 0x800000)
      VArray2_LockRegion(&vram, block->start + VRAM_SIZE, block->len);

   vramlock_list_add(block);

#ifndef TARGET_NO_THREADS
   slock_unlock(vramlist_lock);
#endif

	return block;
}


bool VramLockedWrite(u8* address)
{
   size_t offset=address-vram.data;

   if (offset<VRAM_SIZE)
   {

      size_t addr_hash = offset/PAGE_SIZE;
      vector<vram_block*>* list=&VramLocks[addr_hash];

#ifndef TARGET_NO_THREADS
      slock_lock(vramlist_lock);
#endif

      for (size_t i=0;i<list->size();i++)
      {
         if ((*list)[i])
         {
            libPvr_LockedBlockWrite((*list)[i],(u32)offset);

            if ((*list)[i])
            {
               msgboxf("Error : pvr is supposed to remove lock",MBX_OK);
               dbgbreak;
            }

         }
      }
      list->clear();

      VArray2_UnLockRegion(&vram, (u32)offset&(~(PAGE_SIZE-1)),PAGE_SIZE);

      //TODO: Fix this for 32M wrap as well
      if (_nvmem_enabled() && VRAM_SIZE == 0x800000)
         VArray2_UnLockRegion(&vram, (u32)offset&(~(PAGE_SIZE-1)) + VRAM_SIZE,PAGE_SIZE);

#ifndef TARGET_NO_THREADS
      slock_unlock(vramlist_lock);
#endif

      return true;
   }

   return false;
}

void libCore_vramlock_Free(void)
{
#ifndef TARGET_NO_THREADS
   slock_free(vramlist_lock);
   vramlist_lock = NULL;
#endif
}

void libCore_vramlock_Init(void)
{
#ifndef TARGET_NO_THREADS
   vramlist_lock = slock_new();
#endif
}

//unlocks mem
//also frees the handle
void libCore_vramlock_Unlock_block(vram_block* block)
{
#ifndef TARGET_NO_THREADS
	slock_lock(vramlist_lock);
#endif
	libCore_vramlock_Unlock_block_wb(block);
#ifndef TARGET_NO_THREADS
	slock_unlock(vramlist_lock);
#endif
}

void libCore_vramlock_Unlock_block_wb(vram_block* block)
{
	if (block->end <= VRAM_SIZE)
	{
		vramlock_list_remove(block);
		//more work needed
		free(block);
	}
}
