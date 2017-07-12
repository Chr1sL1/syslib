#ifndef __misc_h__
#define __misc_h__

unsigned long align_to_2power_top(unsigned long val);
unsigned long align_to_2power_floor(unsigned long val);

unsigned long rdtsc(void);
unsigned long log_2(unsigned long val);
long is_2power(unsigned long val);

unsigned long align8(unsigned long val);
unsigned long align16(unsigned long val);

long quick_mmcpy_a(void* dst, void* src, long size);
long quick_mmcpy_u(void* dst, void* src, long size);
long quick_mmcpy(void* dst, void* src, long size);

#endif
