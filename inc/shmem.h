#ifndef __shmem_h__
#define __shmem_h__

#define MAX_SHMM_NAME_LEN (255)

struct shmm_blk
{
	void* addr_begin;
	void* addr_end;
	long key;
};

struct shmm_blk* shmm_create(const char* shmm_name, long channel, unsigned long size, long try_huge_page);
struct shmm_blk* shmm_open(const char* shmm_name, long channel, void* at_addr);

long shmm_close(struct shmm_blk** shmb);
long shmm_destroy(struct shmm_blk** shmb);

#endif
