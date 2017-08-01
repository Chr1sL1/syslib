#ifndef __mmpool_h__
#define __mmpool_h__

struct mmpool
{
	void* addr_begin;
	void* addr_end;
};

struct mmpool* mmp_create(void* addr, unsigned long size, unsigned int min_block_order, unsigned int max_block_order);
struct mmpool* mmp_load(void* addr);
void mmp_destroy(struct mmpool* mmp);

void* mmp_alloc(struct mmpool* mmp, unsigned long size);
long mmp_free(struct mmpool* mmp, void* p);

long mmp_check(struct mmpool* mmp);
long mmp_freelist_profile(struct mmpool* mmp);


#endif

