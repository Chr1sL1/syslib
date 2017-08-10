#ifndef __mmspace_h__
#define __mmspace_h__

#include "dlist.h"

struct shmm_blk;
struct mm_zone_config;

struct mm_chunk
{
	struct shmm_blk* shm_blk;
	unsigned long remain_size;
};

struct mm_chunk* mm_chunk_create(long key, unsigned long size, int try_huge_page);
struct mm_chunk* mm_chunk_load(long key, void* at_addr);

long mm_chunk_destroy(struct mm_chunk* mmc);

long mm_chunk_add_zone(struct mm_chunk* mmc, long type, unsigned long size, struct mm_zone_config* cfg);


long mm_initialize(void);
long mm_load(const char* mm_inf_file);
long mm_save(const char* mm_inf_file);
long mm_uninitialize(void);

void* mm_alloc(unsigned long size, int alloc_flag);
long mm_free(void* p);


#endif

