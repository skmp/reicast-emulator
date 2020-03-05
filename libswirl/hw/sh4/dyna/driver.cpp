/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/mem/_vmem.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/gdrom/gdrom_if.h"

#include <time.h>
#include <float.h>

#include "blockmanager.h"
#include "ngen.h"
#include "decoder.h"

#define bm_printf(...)

#if FEAT_SHREC != DYNAREC_NONE

u8 SH4_TCB[CODE_SIZE+4096]
#if HOST_OS == OS_WINDOWS || FEAT_SHREC != DYNAREC_JIT
	;
#elif HOST_OS == OS_LINUX
	__attribute__((section(".text")));
#elif HOST_OS==OS_DARWIN
	__attribute__((section("__TEXT,.text")));
#else
	#error SH4_TCB ALLOC
#endif

u8* CodeCache;
uintptr_t cc_rx_offset;

u32 LastAddr;
u32 LastAddr_min;
u32* emit_ptr=0;
NGenBackend* rdv_ngen;

void* emit_GetCCPtr() { return emit_ptr==0?(void*)&CodeCache[LastAddr]:(void*)emit_ptr; }
void emit_SetBaseAddr() { LastAddr_min = LastAddr; }
void emit_WriteCodeCache()
{
	wchar path[512];
	sprintf(path,"/code_cache_%8p.bin",CodeCache);
	string pt2=get_writable_data_path(path);
	printf("Writing code cache to %s\n",pt2.c_str());
	FILE*f=fopen(pt2.c_str(),"wb");
	if (f)
	{
		fwrite(CodeCache,LastAddr,1,f);
		fclose(f);
		printf("Written!\n");
	}

	bm_WriteBlockMap(pt2+".map");
}

void RASDASD()
{
	LastAddr=LastAddr_min;
	memset(emit_GetCCPtr(),0xCC,emit_FreeSpace());
}

void emit_Write32(u32 data)
{
	if (emit_ptr)
	{
		*emit_ptr=data;
		emit_ptr++;
	}
	else
	{
		*(u32*)&CodeCache[LastAddr]=data;
		LastAddr+=4;
	}
}

void emit_Skip(u32 sz)
{
	LastAddr+=sz;
}
u32 emit_FreeSpace()
{
	return CODE_SIZE-LastAddr;
}


SmcCheckEnum DoCheck(u32 pc, u32 len)
{

	// is on bios or such
	if (!GetMemPtr(pc, len))
	{
		return NoCheck;
	}
	
	// no need for checks if fault based discard is used for this block
	if (!bm_RamPageHasData(pc, len))
	{
		// printf("FAST CHECK %08X, %d\n", pc, len);

		return NoCheck; //FaultCheck; //ValidationCheck;
	}
	else
	{
		// printf("SLOW CHECK %08X, %d\n", pc, len);		
	}

	// if no fault based discards, use whatever options
	switch (settings.dynarec.SmcCheckLevel) {

		// Heuristic-elimintaed FastChecks
		case NoCheck: {
			if (IsOnRam(pc))
			{
				pc&=0xFFFFFF;
				switch(pc)
				{
					//DOA2LE
					case 0x3DAFC6:
					case 0x3C83F8:

					//Shenmue 2
					case 0x348000:
						
					//Shenmue
					case 0x41860e:
					

						return FastCheck;

					default:
						return NoCheck;
				}
			}
			return NoCheck;
		}
		break;

		// Fast Check everything
		case FastCheck:
			return FastCheck;

		// Full Check everything
		case FullCheck:
			return FullCheck;

		default:
			die("Unhandled settings.dynarec.SmcCheckLevel");
			return FullCheck;
	}
}

void AnalyseBlock(RuntimeBlockInfo* blk);

char block_hash[1024];

#include "deps/crypto/sha1.h"

const char* RuntimeBlockInfo::hash(bool full, bool relocable)
{
	sha1_ctx ctx;
	sha1_init(&ctx);

	u8* ptr = GetMemPtr(this->addr,this->guest_opcodes*2);

	if (ptr)
	{
		if (relocable)
		{
			for (u32 i=0; i<this->guest_opcodes; i++)
			{
				u16 data=ptr[i];
				//Do not count PC relative loads (relocated code)
				if ((ptr[i]>>12)==0xD)
					data=0xD000;

				sha1_update(&ctx,2,(u8*)&data);
			}
		}
		else
		{
			sha1_update(&ctx,this->guest_opcodes*2,ptr);
		}
	}

	sha1_final(&ctx);

	if (full)
		sprintf(block_hash,">:%d:%08X:%02X:%08X:%08X:%08X:%08X:%08X",relocable,this->addr,this->guest_opcodes,ctx.digest[0],ctx.digest[1],ctx.digest[2],ctx.digest[3],ctx.digest[4]);
	else
		sprintf(block_hash,">:%d:%02X:%08X:%08X:%08X:%08X:%08X",relocable,this->guest_opcodes,ctx.digest[0],ctx.digest[1],ctx.digest[2],ctx.digest[3],ctx.digest[4]);

	//return ctx
	return block_hash;
}

void RuntimeBlockInfo::Setup(u32 rpc,fpscr_t rfpu_cfg)
{
	staging_runs=addr=lookups=runs=host_code_size=0;
	guest_cycles=guest_opcodes=host_opcodes=0;
	pBranchBlock=pNextBlock=0;
	code=0;
	has_jcond=false;
	BranchBlock=NextBlock=csc_RetCache=0xFFFFFFFF;
	BlockType=BET_SCL_Intr;
	
	addr=rpc;
	fpu_cfg=rfpu_cfg;
	
	oplist.clear();

	dec_DecodeBlock(this,SH4_TIMESLICE/2);
	AnalyseBlock(this);
}

DynarecCodeEntryPtr rdv_CompilePC_OrFail(bool soft_resets)
{
	u32 pc=next_pc;

	bm_printf("rdv_CompilePC: %08X\n", next_pc);


	if (soft_resets)
	{
		if (pc==0x8c0000e0 || pc==0xac010000 || pc==0xac008300)
		{
			bm_printf("rdv_CompilePC: failed (soft jit reset) %08X\n", next_pc);	 	
			return nullptr;
		}
	}

	if (emit_FreeSpace()<16*1024)
	{
		bm_printf("rdv_CompilePC: failed out of mem %08X\n", next_pc);
		return nullptr;
	}

	RuntimeBlockInfo* rv=0;
	do
	{
		RuntimeBlockInfo* rbi = rdv_ngen->AllocateBlock();
		if (rv==0) rv=rbi;

		rbi->Setup(pc,fpscr);

		
		bool do_opts=((rbi->addr&0x3FFFFFFF)>0x0C010100);
		rbi->staging_runs=do_opts?100:-100;

		rdv_ngen->Compile(rbi,DoCheck(rbi->addr, rbi->sh4_code_size),(pc&0xFFFFFF)==0x08300 || (pc&0xFFFFFF)==0x10000,false,do_opts);

		verify(rbi->code!=0);

		bool doLock = !bm_RamPageHasData(rbi->addr, rbi->sh4_code_size); // && maybe some setting?

		bm_AddBlock(rbi, doLock);

		if (rbi->BlockType==BET_Cond_0 || rbi->BlockType==BET_Cond_1)
			pc=rbi->NextBlock;
		else
			pc=0;
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunreachable-code"
	} while(false && pc);
    #pragma clang diagnostic pop

	bm_printf("rdv_CompilePC: end %08X\n", next_pc);

	return rv->code;
}

DynarecCodeEntryPtr rdv_FindOrCompile_OrFail()
{
        DynarecCodeEntryPtr rv = bm_GetCode(next_pc);  // Returns exec addr
        if (rv == rdv_ngen->FailedToFindBlock)
        {
        	rv = rdv_CompilePC_OrFail(true);
        	
        	if (rv == nullptr)
        		return nullptr;

            rv = (DynarecCodeEntryPtr)CC_RW2RX(rv);  // Returns rw addr
        }
       
        return rv;
}

DynarecCodeEntryPtr rdv_CompilePC_OrClearCache()
{
	auto rv = rdv_CompilePC_OrFail(true);

	if (!rv)
	{
        sh4_cpu->ResetCache();

		rv = rdv_CompilePC_OrFail(false);

		verify(rv != nullptr);
	}

	return rv;
}

DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc)
{
	//printf("rdv_FailedToFindBlock ~ %08X\n",pc);
	next_pc=pc;

	return (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC_OrClearCache());
}

u32 DYNACALL rdv_DoInterrupts_pc(u32 pc) {
	next_pc = pc;
	UpdateINTC();

	//We can only safely relocate/etc stuff here, as in other generic update cases
	//There's a RET, meaning the code can't move around
	//Interrupts happen at least 50 times/second, so its not a problem ..

#if 0 // bm-extras not working anymore
	if (rebuild_counter == 0)
	{
		// TODO: Why is this commented, etc.
		//bm_Rebuild();
	}
#endif

	return next_pc;
}

void bm_Rebuild();
u32 DYNACALL rdv_DoInterrupts(void* block_cpde)
{
	RuntimeBlockInfo* rbi = bm_GetBlock(block_cpde);
	return rdv_DoInterrupts_pc(rbi->addr);
}

DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 pc)
{
	next_pc=pc;
	auto block = bm_GetBlock(pc);

	printf("Discard: %08X, %p\n", pc, block);

	bm_DiscardBlock(block);
	return (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC_OrClearCache());
}

void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc)
{
	bm_printf("rdv_LinkBlock()\n");
	// code is the RX addr to return after, however bm_GetBlock returns RW
	RuntimeBlockInfo* rbi = bm_GetBlock(code);


	bool do_link = true;

	if (!rbi)
	{
		printf("Stale block ..\n");
		rbi = bm_GetStaleBlock(code);
		do_link = false;
	}
	
	verify(rbi != NULL);

	u32 bcls=BET_GET_CLS(rbi->BlockType);

	if (bcls==BET_CLS_Static)
	{
		next_pc=rbi->BranchBlock;
	}
	else if (bcls==BET_CLS_Dynamic)
	{
		next_pc=dpc;
	}
	else if (bcls==BET_CLS_COND)
	{
		if (dpc)
			next_pc=rbi->BranchBlock;
		else
			next_pc=rbi->NextBlock;
	}

	DynarecCodeEntryPtr rv = rdv_FindOrCompile_OrFail();  // Returns rx ptr or NULL

	if (rv != nullptr)
	{
		RuntimeBlockInfo* nxt=bm_GetBlock(next_pc);

		if (nxt)
		{
			if (do_link)
			{
				if (bcls==BET_CLS_Dynamic)
				{
					// this is broken for now, I don't think it is used
					verify(rbi->relink_data==0 || rbi->pBranchBlock==0);

					if (rbi->pBranchBlock!=0)
					{
						rbi->pBranchBlock->RemRef(rbi);
						rbi->pBranchBlock=0;
						rbi->relink_data=1;
					}
					else if (rbi->relink_data==0)
					{
						rbi->pBranchBlock=nxt;
						nxt->AddRef(rbi);
					}
				}
				else
				{
				
					if (rbi->BranchBlock==next_pc)
						rbi->pBranchBlock=nxt;
					if (rbi->NextBlock==next_pc)
						rbi->pNextBlock=nxt;

					nxt->AddRef(rbi);
				}
				u32 ncs=rbi->relink_offset+rbi->Relink();
				verify(rbi->host_code_size>=ncs);
				rbi->host_code_size=ncs;
			}
			else
			{
				printf(" .. null RBI: %08X -- unlinked stale block\n",next_pc);
			}
		}
	}
	else
	{
		rv = rdv_CompilePC_OrClearCache();
	}

	bm_printf("rdv_LinkBlock() 2\n");
	bm_CleanupDeletedBlocks();	// get rid of the old RBI, it is not needed any more
	
	bm_printf("rdv_LinkBlock() end\n");
	return (void*)rv;
}
struct recSH4 : SuperH4Backend {
	~recSH4() { Term(); }

    void ClearCache()
    {
        LastAddr = LastAddr_min;
        bm_Reset();

        printf("recSh4:Dynarec Cache clear at %08X\n", curr_pc);
    }

    void Loop()
    {
        sh4_dyna_rcb = (u8*)& Sh4cntx + sizeof(Sh4cntx);
        printf("cntx // fpcb offset: %td // pc offset: %td // pc %08X\n", (u8*)& sh4rcb.fpcb - sh4_dyna_rcb, (u8*)& sh4rcb.cntx.pc - sh4_dyna_rcb, sh4rcb.cntx.pc);

        if (!settings.dynarec.safemode)
            printf("Warning: Dynarec safe mode is off\n");

        if (settings.dynarec.unstable_opt)
            printf("Warning: Unstable optimizations is on\n");

        if (settings.dynarec.SmcCheckLevel != FullCheck)
            printf("Warning: SMC check mode is %d\n", settings.dynarec.SmcCheckLevel);

        verify(rcb_noffs(&next_pc) == -184);

        rdv_ngen->Mainloop(sh4_dyna_rcb);
    }


    bool Init()
    {
        printf("recSh4 Init\n");
        
        bm_Init();
        bm_Reset();

        verify(rcb_noffs(p_sh4rcb->fpcb) == FPCB_OFFSET);

        verify(rcb_noffs(p_sh4rcb->sq_buffer) == -512);

        verify(rcb_noffs(&p_sh4rcb->cntx.sh4_sched_next) == -152);
        verify(rcb_noffs(&p_sh4rcb->cntx.interrupt_pend) == -148);

        if (_nvmem_enabled()) {
            verify(sh4_cpu->mram.data == ((u8*)p_sh4rcb->sq_buffer + 512 + 0x0C000000));
        }

        // Prepare some pointer to the pre-allocated code cache:
        void* candidate_ptr = (void*)(((unat)SH4_TCB + 4095) & ~4095);

        // Call the platform-specific magic to make the pages RWX
        CodeCache = NULL;
#ifdef FEAT_NO_RWX_PAGES
        verify(vmem_platform_prepare_jit_block(candidate_ptr, CODE_SIZE, (void**)& CodeCache, &cc_rx_offset));
#else
        verify(vmem_platform_prepare_jit_block(candidate_ptr, CODE_SIZE, (void**)& CodeCache));
#endif
        // Ensure the pointer returned is non-null
        verify(CodeCache != NULL);

        memset(CodeCache, 0xFF, CODE_SIZE);
        verify(rdv_ngen->Init());

        bm_Reset();

        return true;
    }

    void Term()
    {
        printf("recSh4 Term\n");
        bm_Term();
    }
};

SuperH4Backend*  Get_Sh4Recompiler()
{
    return new recSH4();
}

bool rdv_RegisterShilBackend(const ngen_backend_t& backend)
{
	verify(rdv_ngen == nullptr);

	rdv_ngen = backend.Create();
	
	return true;
};

#endif  // FEAT_SHREC != DYNAREC_NONE
