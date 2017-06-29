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
