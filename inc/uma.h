#ifndef __uma_h__
#define __uma_h__

struct uma
{
	void* addr_begin;
	void* addr_end;
	unsigned long obj_size;
};

struct uma* uma_create(void* addr, unsigned long size, unsigned long obj_size);
struct uma* uma_load(void* addr);

long uma_destroy(struct uma* mm);

void* uma_alloc(struct uma* mm);
long uma_free(struct uma* mm, void* p);

long uma_check(struct uma* mm);

#endif

