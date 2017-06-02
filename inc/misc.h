#ifndef __misc_h__
#define __misc_h__

int align_to_2power(unsigned int val);
unsigned long rdtsc(void);
long log_2(long val);
int is_2power(unsigned int val);

void* align16(void* p);

#endif
