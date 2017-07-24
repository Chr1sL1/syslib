#ifndef __shmem_h__
#define __shmem_h__

struct shmm_blk
{
	void* addr;
	long size;
};

struct shmm_blk* shmm_new(const char* shmm_name, long channel, long size, long try_huge_page);
struct shmm_blk* shmm_open(const char* shmm_name, long channel, void* at_addr);

long shmm_close(struct shmm_blk** shmb);
long shmm_del(struct shmm_blk** shmb);

#endif
