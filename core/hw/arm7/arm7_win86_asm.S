.section .text
.intel_syntax noprefix


.extern CompileCode
.globl CPUFiq
.globl EntryPoints

###########################################

.globl _arm_compilecode
_arm_compilecode:
	call _CompileCode
   mov eax,0
	jmp arm_dispatch

###########################################
#define R15_ARM_NEXT 46
#define CYCL_CNT 48

.globl _arm_mainloop
_arm_mainloop:
	push esi

   mov esi,ecx
   add esi, reg[CYCL_CNT * 4].I

   mov eax, 0
	jmp arm_dispatch

###########################################

.globl _arm_dispatch
_arm_dispatch:

arm_disp:
   mov eax,reg[R15_ARM_NEXT*4].I
   and eax,0x1FFFFC
	cmp reg[INTR_PEND*4].I,0
	jne arm_dofiq
	jmp [_EntryPoints+eax]

arm_dofiq:
	call CPUFiq
	jmp arm_disp

###########################################

.globl _arm_exit
_arm_exit:
	mov reg[CYCL_CNT*4].I,esi
	pop esi
	ret
