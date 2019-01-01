.text

.global syscall
syscall:
	movq $0, %rax
	movq %rcx, %r10
	syscall
	jb err
	retq
err:
	pushq %rax
	callq __error
	popq %rcx
	movl %ecx, 0(%rax)
	movq $0xFFFFFFFFFFFFFFFF, %rax
	movq $0xFFFFFFFFFFFFFFFF, %rdx
	retq
