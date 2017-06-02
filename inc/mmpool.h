#ifndef __mmpool_h__
#define __mmpool_h__

struct mmpool
{
	void* mm_addr;
	long mm_size;
};

struct mmpool* mmp_new(void* addr, long size);
void mmp_del(struct mmpool* mmp);

void* mmp_alloc(long size);
void mmp_free(void* p);


#endif

