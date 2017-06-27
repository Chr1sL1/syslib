	.text
	.globl	align_to_2power_top
	.type	align_to_2power_top, @function
align_to_2power_top:
	.cfi_startproc
	xorq	%rax, %rax
	testq	%rdi, %rdi
	je		.ALIGN_TO_2POWER_TOP_RET
	bsrq	%rdi, %rcx
	bsfq	%rdi, %rdx
	leaq	0x1(%rcx), %rsi
	cmpq	%rcx, %rdx
	cmovneq	%rsi, %rcx
	movq	$1, %rax
	salq	%cl, %rax
.ALIGN_TO_2POWER_TOP_RET:
	ret
	.cfi_endproc

	.globl	align_to_2power_floor
	.type	align_to_2power_floor, @function
align_to_2power_floor:
	.cfi_startproc
	xorq	%rax, %rax
	testq	%rdi, %rdi
	je		.ALIGN_TO_2POWER_FLOOR_RET
	bsrq	%rdi, %rcx
	leaq	0x1(%rcx), %rsi
	movq	$1, %rax
	salq	%cl, %rax
.ALIGN_TO_2POWER_FLOOR_RET:
	ret
	.cfi_endproc

	.globl	is_2power
	.type	is_2power, @function
is_2power:
	.cfi_startproc
	xorq	%rax, %rax
	bsrq	%rdi, %rcx
	bsfq	%rdi, %rdx
	leaq	0x1(%rcx), %rsi
	cmpq	%rcx, %rdx
	sete	%al
	ret
	.cfi_endproc

	.globl	align8
	.type	align8, @function
align8:
	.cfi_startproc
	addq	$8, %rdi
	andq	$-8, %rdi
	movq	%rdi, %rax
	ret
	.cfi_endproc


	.globl	align16
	.type	align16, @function
align16:
	.cfi_startproc
	addq	$16, %rdi
	andq	$-16, %rdi
	movq	%rdi, %rax
	ret
	.cfi_endproc

	.globl	rdtsc
	.type	rdtsc, @function
rdtsc:
	.cfi_startproc
	rdtsc
	salq	$32, %rdx
	addq	%rdx, %rax
	ret
	.cfi_endproc

	.globl	log_2
	.type	log_2, @function
log_2:
	.cfi_startproc
	bsrq	%rdi, %rax
	ret
	.cfi_endproc


	.globl	quick_mmcpy_a	
	.type	quick_mmcpy_a, @function
quick_mmcpy_a:
	.cfi_startproc

.Q_MMCPY_START:
	testq	%rdx, %rdx
	jle		.Q_MMCPY_FINISH
	movdqa	(%rsi), %xmm0
	movdqa	%xmm0, (%rdi)
	addq	$0x10, %rsi
	addq	$0x10, %rdi
	subq	$0x10, %rdx
	jmp		.Q_MMCPY_START
.Q_MMCPY_FINISH:
	ret
	.cfi_endproc

