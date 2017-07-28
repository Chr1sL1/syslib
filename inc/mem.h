#ifndef __mem_h__
#define __mem_h__


struct mem_space
{
	void* addr_begin;
	void* addr_end;
};

struct mem_zone
{
	unsigned long type;
	void* addr_begin;
	void* addr_end;
};

struct mem_space* mm_space_create(void* addr, unsigned long size);
void mm_space_destroy(struct mem_space* mms);

struct mem_zone* mm_zone_create(struct mem_space* mms, unsigned long size, unsigned long type);


#endif


