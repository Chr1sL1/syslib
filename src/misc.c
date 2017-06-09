#include "misc.h"


unsigned int align_to_2power_top(unsigned int val)
{
	__asm__("bsrl	%edi, %ecx\n"\
			"bsfl	%edi, %edx\n"\
			"leal	0x1(%ecx), %esi\n"\
			"cmpl	%ecx, %edx\n"\
			"cmovnel	%esi, %ecx\n"\
			"movl	$1, %eax\n"\
			"sall	%cl, %eax\n");
}

unsigned int align_to_2power_floor(unsigned int val)
{
	__asm__("bsrl	%edi, %ecx\n"\
			"leal	0x1(%ecx), %esi\n"\
			"movl	$1, %eax\n"\
			"sall	%cl, %eax\n");
}

int is_2power(unsigned int val)
{
	__asm__("xorl	%eax, %eax\n"\
			"bsrl	%edi, %ecx\n"\
			"bsfl	%edi, %edx\n"\
			"leal	0x1(%ecx), %esi\n"\
			"cmpl	%ecx, %edx\n"\
			"sete	%al\n");
}

unsigned int align8(unsigned val)
{
	__asm__("addl	$8, %edi\n"\
			"andl	$-8, %edi\n"\
			"movl	%edi, %eax");
}

void* align16(void* p)
{
	__asm__("addq	$16, %rdi\n"\
			"andq	$-16, %rdi\n"\
			"movq	%rdi, %rax");
}

unsigned long rdtsc(void)
{
	__asm__("rdtsc\n"\
			"salq	$32, %rdx\n"\
			"addq	%rdx, %rax");
}

long log_2(long val)
{
	__asm__("bsrq	%rdi, %rax\n");
}
