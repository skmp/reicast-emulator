#pragma once
#include "types.h"
#include "sh4/dyna/shil.h"

void prof_init();
void prof_periodical();

inline void print_array(const wchar_t* name, u32* arr,u32 size)
{
	wprintf(L"%s = [",name);
	for(u32 i=0; i<size; i++)
	{
		wprintf(L"%d ",arr[i]);
		if (i%12==0)
			wprintf(L" ...\n");
	}
	wprintf(L"];\n");
}

inline void print_elem(const wchar_t* name, u32 val)
{
	wprintf(L"%s=%d;\n",name,val);
}

inline void print_head(const wchar_t* name) { wprintf(L"//<%s>\n",name); }

struct profiler_cfg
{
	bool enable;

	struct 
	{
		struct
		{
			u32 executed[shop_max];
			u32 host_ops[shop_max];

			u32 readm_const;
			u32 readm_reg;
			u32 readm_reg_imm;
			u32 readm_reg_reg;

			u32 pref_same;
			u32 writem_const[4];
			u32 writem_reg[4];
			u32 writem_reg_imm[4];

			/*
				other statistics about opcodes
			*/

			void print()
			{
				print_head(L"shil");

				print_array(L"executed",executed,shop_max);
				print_array(L"host_ops",host_ops,shop_max);
				
				print_elem(L"readm_const",readm_const);
				print_elem(L"readm_reg",readm_reg);
				print_elem(L"readm_reg_imm",readm_reg_imm);
				print_elem(L"readm_reg_reg",readm_reg_reg);
			}

		} shil;

		struct
		{
			//mem,reg || r,w,rw
			u32 reg_r[sh4_reg_count];
			u32 reg_w[sh4_reg_count];
			u32 reg_rw[sh4_reg_count];
			
			void print()
			{
				print_head(L"ralloc");
				print_array(L"reg_r",reg_r,sh4_reg_count);
				print_array(L"reg_w",reg_w,sh4_reg_count);
				print_array(L"reg_rw",reg_rw,sh4_reg_count);
			}
		} ralloc;

		struct
		{
			u32 cached;
			u32 linked_cond;	//cond, direct, indirect
			u32 linked_direct;
			u32 linked_indirect;
			u32 callstack_hit;
			u32 callstack_miss;
			u32 slowpath;

			void print() 
			{ 
				print_head(L"bm");
				print_elem(L"bm_cache",cached);
				print_elem(L"linked_cond",linked_cond);
				print_elem(L"linked_direct",linked_direct);
				print_elem(L"linked_indirect",linked_indirect);
				print_elem(L"callstack_hit",callstack_hit);
				print_elem(L"callstack_miss",callstack_miss);
				print_elem(L"slowpath",slowpath);
			}
		} bm;

		struct
		{
			u32 call_direct; u32 call_indirect;
			u32 jump_direct; u32 jump_indirect;
			
			u32 cond[2];		//conditional false, true
			u32 ret;		//only indirect

			u32 force_check;
			u32 cycles[512];

			void print()
			{
				print_head(L"blkrun");

				print_elem(L"call_direct",call_direct);
				print_elem(L"call_indirect",call_indirect);
				print_elem(L"jump_direct",jump_direct);
				print_elem(L"jump_indirect",jump_indirect);
				print_elem(L"cond0",cond[0]);
				print_elem(L"cond1",cond[1]);
				print_elem(L"ret",ret);

				print_elem(L"force_check",force_check);

				print_array(L"cycles",cycles,512);

			}
		} blkrun;

		void print()
		{
			shil.print();
			ralloc.print();
			bm.print();
			blkrun.print();
		}
	} counters;
};

extern profiler_cfg prof;


void install_prof_handler(int id);
void sample_Start(int freq);
void sample_Stop();
bool sample_Switch(int freq);
void sample_Syms(u8* data,u32 len);
