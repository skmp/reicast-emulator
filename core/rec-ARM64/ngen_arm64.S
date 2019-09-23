.hidden ngen_LinkBlock_cond_Branch_stub	
.globl ngen_LinkBlock_cond_Branch_stub		
ngen_LinkBlock_cond_Branch_stub:		
    mov w1, #1							
    b ngen_LinkBlock_Shared_stub		

.hidden ngen_LinkBlock_cond_Next_stub	
.globl ngen_LinkBlock_cond_Next_stub	
ngen_LinkBlock_cond_Next_stub:			
    mov w1, #0							
    b ngen_LinkBlock_Shared_stub		

.hidden ngen_LinkBlock_Generic_stub	
.globl ngen_LinkBlock_Generic_stub	
ngen_LinkBlock_Generic_stub:			
    mov w1, w29							// djump/pc -> in case we need it ..
    //b ngen_LinkBlock_Shared_stub		

.hidden ngen_LinkBlock_Shared_stub	
.globl ngen_LinkBlock_Shared_stub	
ngen_LinkBlock_Shared_stub:			
    mov x0, lr							
    sub x0, x0, #4							// go before the call
    bl rdv_LinkBlock							// returns an RX addr
    br x0

.hidden ngen_FailedToFindBlock_nommu	
.globl ngen_FailedToFindBlock_nommu	
ngen_FailedToFindBlock_nommu:			
    mov w0, w29						
    bl rdv_FailedToFindBlock			
    br x0								

.hidden ngen_FailedToFindBlock_mmu	
.globl ngen_FailedToFindBlock_mmu	
ngen_FailedToFindBlock_mmu:			
    bl rdv_FailedToFindBlock_pc		
    br x0								

.hidden ngen_blockcheckfail		
.globl ngen_blockcheckfail			
ngen_blockcheckfail:					
    bl rdv_BlockCheckFail				
    br x0

.hidden context_switch_aarch64		
.globl context_switch_aarch64
context_switch_aarch64:
    // Setup SP on the Buffer
    MOV SP, X0

    // Restore Regs
    LDR X0, [SP, #0]
    LDR X1, [SP, #8]
    LDR X2, [SP, #16]
    LDR X3, [SP, #24]
    LDR X4, [SP, #32]
    LDR X5, [SP, #40]
    LDR X6, [SP, #48]
    LDR X7, [SP, #56]
    LDR X8, [SP, #64]
    LDR X9, [SP, #72]
    LDR X10, [SP, #80]
    LDR X11, [SP, #88]
    LDR X12, [SP, #96]
    LDR X13, [SP, #104]
    LDR X14, [SP, #112]
    LDR X15, [SP, #120]
    LDR X16, [SP, #128]
    LDR X17, [SP, #136]
    LDR X18, [SP, #144]
    LDR X19, [SP, #152]
    LDR X20, [SP, #160]
    LDR X21, [SP, #168]
    LDR X22, [SP, #176]
    LDR X23, [SP, #184]
    LDR X24, [SP, #192]
    LDR X25, [SP, #200]
    LDR X26, [SP, #208]
    LDR X27, [SP, #216]
    LDR X28, [SP, #224]

    // Restore FP, LR, SP
    LDR FP, [SP, #232]
    LDR LR, [SP, #240]
    MOV X15, SP
    LDR X17, [X15, #248]
    MOV SP, X17

    // Branch to PC
    LDR X17, [X15, #256]
    BR X17
