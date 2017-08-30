#ifndef __misc_h__
#define __misc_h__

unsigned long align_to_2power_top(unsigned long val);
unsigned long align_to_2power_floor(unsigned long val);

unsigned long rdtsc(void);
unsigned long log_2(unsigned long val);
long is_2power(unsigned long val);

unsigned long align8(unsigned long val);
unsigned long align16(unsigned long val);

unsigned long round_up(unsigned long val, unsigned long boundary);
unsigned long round_down(unsigned long val, unsigned long boundary);

long bsf(unsigned long val);
long bsr(unsigned long val);

long quick_mmcpy_a(void* dst, void* src, unsigned long size);
long quick_mmcpy_u(void* dst, void* src, unsigned long size);
long quick_mmcpy(void* dst, void* src, unsigned long size);

void* move_ptr_align8(void* ptr, unsigned long offset);
void* move_ptr_align16(void* ptr, unsigned long offset);
void* move_ptr_align64(void* ptr, unsigned long offset);
void* move_ptr_align128(void* ptr, unsigned long offset);
#endif
