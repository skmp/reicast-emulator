/*
	This file is part of libswirl
*/
#include "license/bsd"


#if 0


bool UDgreaterX ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{	
	return elem1->runs > elem2->runs;
}

bool UDgreaterLOC ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{	
	return elem1->addr < elem2->addr;
}

u32 FindPath(RuntimeBlockInfo* rbi, u32 sa,s32 mc,u32& plc)
{
	if (mc < 0 || rbi==0)
		return 0;

	plc++;
	if (rbi->BlockType==BET_Cond_0 || rbi->BlockType==BET_Cond_1)
	{
		u32 plc1=plc,plc2=plc,v1=0,v2=0;
		if (rbi->BranchBlock>sa)
		{
			v1=FindPath(bm_GetBlock(rbi->BranchBlock),rbi->addr,mc-rbi->guest_cycles,plc1);
		}
		v2=FindPath(bm_GetBlock(rbi->NextBlock),rbi->addr,mc-rbi->guest_cycles,plc2);
		if (plc1>plc2)
		{
			plc=plc1;
			return rbi->guest_cycles+v1;
		}
		else
		{
			plc=plc2;
			return rbi->guest_cycles+v2;
		}
		
	}
	else if (rbi->BlockType==BET_StaticJump)
	{
		if (rbi->BranchBlock>sa)
			return rbi->guest_cycles+FindPath(bm_GetBlock(rbi->BranchBlock),rbi->addr,mc-rbi->guest_cycles,plc);
		else
		{
			return rbi->guest_cycles;
		}
	}
	else
	{
		if (plc!=1)
			printf("Chain lost due to %d\n",rbi->BlockType);
		else
			printf("Chain fail due to %d\n",rbi->BlockType);
		return rbi->guest_cycles;
	}
}
u32 total_saved;
void FindPath(u32 start)
{
	RuntimeBlockInfo* rbi=bm_GetBlock(start);

	if (!rbi || !rbi->runs)
		return;

	u32 plen=0;
	u32 pclc=FindPath(rbi,start,SH4_TIMESLICE,plen);
	if (plen>1)
	{
		total_saved+=(plen-1)*2*rbi->runs;
		printf("%08X: %d, %d, %.2f, %.2f\n",start,pclc,plen,pclc/(float)plen,plen*2*rbi->runs/1000.f);
	}
	rbi->runs=0;
}


u32 rebuild_counter=20;
void bm_Periodical_1s()
{
	for (u32 i=0;i<del_blocks.size();i++)
		delete del_blocks[i];

	del_blocks.clear();

	if (rebuild_counter>0) rebuild_counter--;
#if HOST_OS==OS_WINDOWS && 0
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreaterX);

	map<u32,u32> vmap;
	map<u32,u32> calls;

	u32 total_runs=0;
	for(int i=0;i<all_blocks.size();i++)
	{
		RuntimeBlockInfo* rbi=all_blocks[i];
		total_runs+=rbi->runs;
		if (rbi->BranchBlock!=-1 && rbi->BranchBlock < rbi->addr)
		{
			if (rbi->BlockType==BET_Cond_0 || rbi->BlockType==BET_Cond_1 || rbi->BlockType==BET_StaticJump)
			{
				RuntimeBlockInfo* bbi=bm_GetBlock(all_blocks[i]->BranchBlock);
				if (bbi && bbi->runs)
				{
					vmap[all_blocks[i]->BranchBlock]=bbi->runs;
				}
			}

			if (rbi->BlockType==BET_StaticCall)
			{
				RuntimeBlockInfo* bbi=bm_GetBlock(all_blocks[i]->BranchBlock);
				if (bbi && bbi->runs)
				{
					calls[all_blocks[i]->BranchBlock]+=rbi->runs;
				}
			}
		}
		verify(rbi->NextBlock>rbi->addr);
	}

	map<u32,u32>::iterator iter=vmap.begin();

	total_saved=0;
	u32 total_l_runs=0;
	while(iter!=vmap.end())
	{
		FindPath(iter->first);
		total_l_runs+=iter->second;
		iter++;
	}

	for(int i=0;i<all_blocks.size();i++)
		all_blocks[i]->runs=0;

	printf("Total Saved: %.2f || Total Loop Runs: %.2f  || Total Runs: %.2f\n",total_saved/1000.f,total_l_runs/1000.f,total_runs/1000.f);
#endif
}


void constprop(RuntimeBlockInfo* blk);
void bm_Rebuild()
{
	return;

	die("this is broken in multiple levels, including compile options");

	void RASDASD();
	RASDASD();

	blkmap.clear();
	
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreaterLOC);
	
	for(size_t i=0; i<all_blocks.size(); i++)
	{
		bool do_opts=((all_blocks[i]->addr&0x3FFFFFFF)>0x0C010100);

		if (all_blocks[i]->staging_runs<0 && do_opts)
		{
//#if HOST_OS==OS_WINDOWS
			//constprop(all_blocks[i]);
//#endif
		}
		ngen_Compile(all_blocks[i],NoCheck,false,all_blocks[i]->staging_runs>0,do_opts);

		blkmap.insert(all_blocks[i]);
		verify(bm_GetBlock((RuntimeBlockInfo*)all_blocks[i]->code)==all_blocks[i]);

		FPCA(all_blocks[i]->addr)=all_blocks[i]->code;
	}

	for(size_t i=0; i<all_blocks.size(); i++)
	{
		all_blocks[i]->Relink();
	}

	rebuild_counter=30;
}

// BM extras
void bm_WriteBlockMap(const string& file)
{
	FILE* f=fopen(file.c_str(),"wb");
	if (f)
	{
		printf("Writing block map !\n");
		for (size_t i=0; i<all_blocks.size(); i++)
		{
			fprintf(f,"block: %d:%08X:%p:%d:%d:%d\n",all_blocks[i]->BlockType,all_blocks[i]->addr,all_blocks[i]->code,all_blocks[i]->host_code_size,all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes);
			for(size_t j=0;j<all_blocks[i]->oplist.size();j++)
				fprintf(f,"\top: %zd:%d:%s\n",j,all_blocks[i]->oplist[j].guest_offs,all_blocks[i]->oplist[j].dissasm().c_str());
		}
		fclose(f);
		printf("Finished writing block map\n");
	}
}

u32 GetLookup(RuntimeBlockInfo* elem)
{
	return elem->lookups;
}

bool UDgreater ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs > elem2->runs;
}

bool UDgreater2 ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs*elem1->host_opcodes > elem2->runs*elem2->host_opcodes;
}

bool UDgreater3 ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs*elem1->host_opcodes/elem1->guest_cycles > elem2->runs*elem2->host_opcodes/elem2->guest_cycles;
}

void sh4_jitsym(FILE* out)
{
	for (size_t i=0; i<all_blocks.size(); i++)
	{
		fprintf(out,"%p %d %08X\n",all_blocks[i]->code,all_blocks[i]->host_code_size,all_blocks[i]->addr);
	}
}

void bm_PrintTopBlocks()
{
	double total_lups=0;
	double total_runs=0;
	double total_cycles=0;
	double total_hops=0;
	double total_sops=0;

	for (size_t i=0;i<all_blocks.size();i++)
	{
		total_lups+=GetLookup(all_blocks[i]);
		total_cycles+=all_blocks[i]->runs*all_blocks[i]->guest_cycles;
		total_hops+=all_blocks[i]->runs*all_blocks[i]->host_opcodes;
		total_sops+=all_blocks[i]->runs*all_blocks[i]->guest_opcodes;
		total_runs+=all_blocks[i]->runs;
	}

	printf("Total lookups:  %.0fKRuns, %.0fKLuops, Total cycles: %.0fMhz, Total Hops: %.0fMips, Total Sops: %.0fMips! \n",total_runs/1000,total_lups/1000,total_cycles/1000/1000,total_hops/1000/1000,total_sops/1000/1000);
	total_hops/=100;
	total_cycles/=100;
	total_runs/=100;

	double sel_hops=0;
	for (size_t i=0;i<(all_blocks.size()/100);i++)
	{
		printf("Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)\n",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	printf(" >-< %.2f%% covered in top 1%% blocks\n",sel_hops/total_hops);

	size_t i;
	for (i=all_blocks.size()/100;sel_hops/total_hops<50;i++)
	{
		printf("Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)\n",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	printf(" >-< %.2f%% covered in top %.2f%% blocks\n",sel_hops/total_hops,i*100.0/all_blocks.size());

}

void bm_Sort()
{
	printf("!!!!!!!!!!!!!!!!!!! BLK REPORT !!!!!!!!!!!!!!!!!!!!n");

	printf("     ---- Blocks: Sorted based on Runs ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater);
	bm_PrintTopBlocks();

	printf("<><><><><><><><><><><><><><><><><><><><><><><><><>\n");

	printf("     ---- Blocks: Sorted based on hops ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater2);
	bm_PrintTopBlocks();

	printf("<><><><><><><><><><><><><><><><><><><><><><><><><>\n");

	printf("     ---- Blocks: Sorted based on wefs ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater3);
	bm_PrintTopBlocks();

	printf("^^^^^^^^^^^^^^^^^^^ END REPORT ^^^^^^^^^^^^^^^^^^^\n");

	for (size_t i=0;i<all_blocks.size();i++)
	{
		all_blocks[i]->runs=0;
	}
}


void print_blocks()
{
	FILE* f=0;

	if (print_stats)
	{
		f=fopen(get_writable_data_path("/blkmap.lst").c_str(),"w");
		print_stats=0;

		printf("Writing blocks to %p\n",f);
	}

	for (size_t i=0;i<all_blocks.size();i++)
	{
		RuntimeBlockInfo* blk=all_blocks[i];

		if (f)
		{
			fprintf(f,"block: %p\n",blk);
			fprintf(f,"addr: %08X\n",blk->addr);
			fprintf(f,"hash: %s\n",blk->hash());
			fprintf(f,"hash_rloc: %s\n",blk->hash(false,true));
			fprintf(f,"code: %p\n",blk->code);
			fprintf(f,"runs: %d\n",blk->runs);
			fprintf(f,"BlockType: %d\n",blk->BlockType);
			fprintf(f,"NextBlock: %08X\n",blk->NextBlock);
			fprintf(f,"BranchBlock: %08X\n",blk->BranchBlock);
			fprintf(f,"pNextBlock: %p\n",blk->pNextBlock);
			fprintf(f,"pBranchBlock: %p\n",blk->pBranchBlock);
			fprintf(f,"guest_cycles: %d\n",blk->guest_cycles);
			fprintf(f,"guest_opcodes: %d\n",blk->guest_opcodes);
			fprintf(f,"host_opcodes: %d\n",blk->host_opcodes);
			fprintf(f,"il_opcodes: %zd\n",blk->oplist.size());

			u32 hcode=0;
			s32 gcode=-1;
			u8* pucode=(u8*)blk->code;

			size_t j=0;
			
			fprintf(f,"{\n");
			for (;j<blk->oplist.size();j++)
			{
				shil_opcode* op=&all_blocks[i]->oplist[j];
				fprint_hex(f,"//h:",pucode,hcode,op->host_offs);

				if (gcode!=op->guest_offs)
				{
					gcode=op->guest_offs;
					u32 rpc=blk->addr+gcode;
					u16 op=ReadMem16(rpc);

					char temp[128];
					OpDesc[op]->Disassemble(temp,rpc,op);

					fprintf(f,"//g:%s\n",temp);
				}

				string s=op->dissasm();
				fprintf(f,"//il:%d:%d:%s\n",op->guest_offs,op->host_offs,s.c_str());
			}
			
			fprint_hex(f,"//h:",pucode,hcode,blk->host_code_size);

			fprintf(f,"}\n");
		}

		all_blocks[i]->runs=0;
	}

	if (f) fclose(f);
}


#endif