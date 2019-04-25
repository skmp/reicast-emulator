/*#
#   util.S util routines in asm, ** PREPROCESSED intel format 
#*/
.intel_syntax noprefix
.text

//# RDI, RSI, RDX, RCX, R8, R9  : R10=RCX 

.global syscall
syscall:
//#
    mov rax,0
    mov r10,rcx
    syscall
    jb err
    ret
err:
    push rax
    call __error
    pop rcx
    mov [rax], ecx
    mov rax,-1
    mov rdx,-1
    ret
//#



.global get_k_addr
get_k_addr:
//#
    mov ecx,0xC0000082
    rdmsr
    shl rdx,32
    or rax,rdx

    sub rax,0x1C0
    add rax,rdi
    ret
//#

.global kcall
kcall:
//#
    push r12
    push r13
    push r14
    mov r12,rax
    mov r13,rcx
    mov r14,rdx

    mov ecx,0xC0000082
    rdmsr
    shl rdx,32
    or rax,rdx

    sub rax,0x1C0
    add rax,r12
    mov rcx,r13
    mov rdx,r14
    pop r14
    pop r13
    pop r12

    jmp rax
//#


.global readmsr     //# u64 readmsr(u32 msr)
readmsr:
    mov ecx,edi
    rdmsr
    shl rdx,32
    or rax,rdx
    ret 
    
.global readcr0     //# u64 readcr0(void)
readcr0:
    mov rax,cr0
    ret

.global writecr0    //# void writecr0(u64 val)
writecr0:
    mov cr0,rax
    ret

.global readcr3     //# u64 readcr3(void)
readcr3:
    mov rax,cr3
    ret
    
.global writecr3    //# void writecr3(u64 val)
writecr3:
    mov cr3,rax
    ret

.global readgs
readgs:
    mov rax,gs
    ret

.global writegs
writegs:
    mov gs,rax
    ret


    
