.section .text
.intel_syntax noprefix


.extern rdv_LinkBlock
.globl _p_sh4rcb
.globl _gas_offs
.globl rdv_FailedToFindBlock
.globl _cycle_counter
.globl _loop_no_update
.globl _intc_sched
.globl _bm_GetCode
.globl _UpdateSystem
.globl _rdv_DoInterrupts
.globl _rdv_BlockCheckFail

###########################################

.globl _ngen_LinkBlock_Shared_stub
_ngen_LinkBlock_Shared_stub:
	pop ecx
	sub ecx,5
	call _rdv_LinkBlock
	jmp eax



###########################################

.globl _ngen_LinkBlock_cond_Next_stub
_ngen_LinkBlock_cond_Next_stub:
	mov edx,0
	jmp _ngen_LinkBlock_Shared_stub
	


###########################################

.globl _ngen_LinkBlock_cond_Branch_stub
_ngen_LinkBlock_cond_Branch_stub:
	mov edx,1
	jmp _ngen_LinkBlock_Shared_stub


###########################################

.globl _ngen_LinkBlock_Generic_stub
# u32 gas_offs=offsetof(Sh4RCB,cntx.jdyn);
_ngen_LinkBlock_Generic_stub:
	mov edx,_p_sh4rcb
	add edx,_gas_offs
	mov edx,[edx]
	jmp _ngen_LinkBlock_Shared_stub




###########################################

.globl _ngen_FailedToFindBlock_
_ngen_FailedToFindBlock_:
	mov ecx,esi
	call _rdv_FailedToFindBlock
	jmp eax



###########################################
#define SH4_TIMESLICE 448

.globl _ngen_mainloop
_ngen_mainloop:
	# quick hack to maintain 16-byte alignment
	push esi
	push esi
	push esi

	push esi
	push edi
	push ebp
	push ebx

        mov ecx,[eax-184]       # PC - was 0xA0000000
	mov dword ptr _cycle_counter, 448 #SH4_TIMESLICE

	mov dword ptr _loop_no_update,offset no_update
	mov dword ptr _intc_sched,offset intc_sched_offs
	
	mov eax,0
		
#	next_pc _MUST_ be on ecx
no_update:
	mov esi,ecx
   call _bm_GetCode #bm_GetCode
	jmp eax

intc_sched_offs:
	add dword ptr _cycle_counter, 448 #SH4_TIMESLICE
	call _UpdateSystem
	cmp eax,0
	jnz do_iter
	ret

do_iter:
	pop ecx
	call _rdv_DoInterrupts
	mov ecx,eax
   mov edx,[_p_sh4rcb];
   add edx,[_cpurun_offset];
   cmp dword ptr [edx],0;
   jz cleanup;
	jmp no_update

cleanup:
	pop ebx
	pop ebp
	pop edi
	pop esi
	
	# quick hack to maintain 16-byte alignment
	pop esi
	pop esi
	pop esi

	ret



###########################################

.globl _ngen_blockcheckfail
_ngen_blockcheckfail:
	call _rdv_BlockCheckFail
	jmp eax


###########################################

.globl _ngen_blockcheckfail2
_ngen_blockcheckfail2:
	int 3
	call _rdv_BlockCheckFail
	jmp eax
