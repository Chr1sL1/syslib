#include "misc.h"


int align_to_2power(unsigned int val)
{
	__asm__("bsrl	%edi, %ecx\n"\
			"bsfl	%edi, %edx\n"\
			"leal	0x1(%ecx), %esi\n"\
			"cmpl	%ecx, %edx\n"\
			"cmovnel	%esi, %ecx\n"\
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

void* align16(void* p)
{
	__asm__("addq	$16, %rdi\n"\
			"andq	$-16, %rdi\n"\
			"movq	%rdi, %rax");
}

unsigned long rdtsc(void)
{
	__asm__("rdtsc\n"\
			"salq	%rdx\n"\
			"addq	%rdx, %rax");
}

long log_2(long val)
{
	__asm__("bsrq	%rdi, %rax\n");
}
