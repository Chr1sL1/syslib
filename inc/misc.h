#ifndef __misc_h__
#define __misc_h__

unsigned long align_to_2power_top(unsigned long val);
unsigned long align_to_2power_floor(unsigned long val);
unsigned long rdtsc(void);
long log_2(long val);
long is_2power(unsigned long val);

void* align8(void* val);
void* align16(void* p);

void quick_mmcpy_a(void* dst, void* src, unsigned long size);
long quick_mmcpy_u(void* dst, void* src, unsigned long size);

#endif
