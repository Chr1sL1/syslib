#ifndef __mmpool_h__
#define __mmpool_h__

struct mmpool
{
	void* mm_addr;
	long mm_size;
};

struct mmpool_config
{
	unsigned int min_block_index;
	unsigned int max_block_index;
};

struct mmpool* mmp_new(void* addr, long size, struct mmpool_config* cfg);
void mmp_del(struct mmpool* mmp);

void* mmp_alloc(struct mmpool* mmp, long size);
long mmp_free(struct mmpool* mmp, void* p);

long mmp_check(struct mmpool* mmp);
long mmp_freelist_profile(struct mmpool* mmp);


#endif

