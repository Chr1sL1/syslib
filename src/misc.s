	.text
	.globl	align_to_2power_top
	.type	align_to_2power_top, @function
align_to_2power_top:
	.cfi_startproc
	xorl	%eax, %eax
	testl	%edi, %edi
	je		.ALIGN_TO_2POWER_TOP_RET
	bsrl	%edi, %ecx
	bsfl	%edi, %edx
	leal	0x1(%ecx), %esi
	cmpl	%ecx, %edx
	cmovnel	%esi, %ecx
	movl	$1, %eax
	sall	%cl, %eax
.ALIGN_TO_2POWER_TOP_RET:
	ret
	.cfi_endproc

	.globl	align_to_2power_floor
	.type	align_to_2power_floor, @function
align_to_2power_floor:
	.cfi_startproc
	xorl	%eax, %eax
	testl	%edi, %edi
	je		.ALIGN_TO_2POWER_FLOOR_RET
	bsrl	%edi, %ecx
	leal	0x1(%ecx), %esi
	movl	$1, %eax
	sall	%cl, %eax
.ALIGN_TO_2POWER_FLOOR_RET:
	ret
	.cfi_endproc

	.globl	is_2power
	.type	is_2power, @function
is_2power:
	.cfi_startproc
	xorl	%eax, %eax
	bsrl	%edi, %ecx
	bsfl	%edi, %edx
	leal	0x1(%ecx), %esi
	cmpl	%ecx, %edx
	sete	%al
	ret
	.cfi_endproc

	.globl	align8
	.type	align8, @function
align8:
	.cfi_startproc
	addl	$8, %edi
	andl	$-8, %edi
	movl	%edi, %eax
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

