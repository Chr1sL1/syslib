#include "misc.h"
#include <string.h>


long quick_mmcpy(void* dst, void* src, long size)
{
	if(size <= 0) goto error_ret;

	long s1 = size - ((long)align16((void*)size) - 16);
	if(s1 > 0)
	{
		memcpy(dst, src, s1);
		dst += s1;
		src += s1;
		size -= s1;
	}

	if(size <= 0) goto succ_ret;

	if(((long)dst & 0xF) == 0 && ((long)src & 0xF) == 0 && (size & 0xF) == 0)
		return quick_mmcpy_a(dst, src, size);

	if((size & 0xF) == 0)
		return quick_mmcpy_u(dst, src, size);

	// ..... todo .....

succ_ret:
	return 0;
error_ret:
	return -1;
}

unsigned long align_to_2power_top(unsigned long val)
{
	unsigned long ret = 0;
	if(val == 0)
		return 0;

	asm("bsrq	%1, %%rcx\n\t"
		"bsfq	%1, %%rdx\n\t"
		"leaq	0x1(%%rcx), %%rsi\n\t"
		"cmpq	%%rcx, %%rdx\n\t"
		"cmovneq	%%rsi, %%rcx\n\t"
		"movq	$1, %0\n\t"
		"salq	%%cl, %0"
		:"=r"(ret)
		:"r"(val));

	return ret;
}

unsigned long align_to_2power_floor(unsigned long val)
{
	unsigned long ret = 0;
	if(val == 0)
		return 0;

	asm("bsrq	%1, %%rcx\n\t"
		"movq	$1, %0\n\t"
		"salq	%%cl, %0"
		:"=r"(ret)
		:"r"(val));

	return ret;
}

long is_2power(unsigned long val)
{
	unsigned long ret = 0;
	if(val == 0)
		return 1;

	asm("bsrq	%1, %%rcx\n\t"
		"bsfq	%1, %%rdx\n\t"
		"cmpq	%%rcx, %%rdx\n\t"
		"sete	%%al\n\t"
		:"=a"(ret)
		:"r"(val));

	return ret;
}

unsigned long rdtsc(void)
{
	unsigned long th, tl;

	asm volatile ("rdtsc":"=a"(tl),"=d"(th));

	return (th << 32) + tl;
}


unsigned long log_2(unsigned long val)
{
	unsigned long ret;
	asm("bsrq	%1, %0":"=r"(ret):"r"(val));

	return ret;
}


unsigned long align8(unsigned long val)
{
	return ((val + 7) & ~7);

}

unsigned long align16(unsigned long val)
{
	return ((val + 15) & ~15);
}

inline void* move_ptr_align8(void* ptr, unsigned long offset)
{
	return (void*)(((unsigned long)(ptr + offset) + 7) & (~7));
}


