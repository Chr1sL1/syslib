#ifndef __misc_h__
#define __misc_h__

unsigned int align_to_2power_top(unsigned int val);
unsigned int align_to_2power_floor(unsigned int val);
unsigned long rdtsc(void);
long log_2(long val);
int is_2power(unsigned int val);

unsigned int align8(unsigned val);
void* align16(void* p);

#endif
