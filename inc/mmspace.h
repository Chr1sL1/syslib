#ifndef __mmspace_h__
#define __mmspace_h__

#include "dlist.h"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmzone: object cache.

struct mmzone
{
	unsigned long obj_size;
	unsigned long current_free_count;
};

struct mmzone* mm_zcreate(unsigned long obj_size);
long mmz_zdestroy(struct mmzone* mmz);

void* mm_zalloc(struct mmzone* mmz);
long mm_zfree(struct mmzone* mmz, void* p);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmspace: a huge block of shared memory in which the whole process's data lays

struct mm_config
{
	unsigned long total_size;
	unsigned long maxpg_count;

	struct mm_buddy_sys
	{
		unsigned long size;
		unsigned int min_block_order;
		unsigned int max_block_order;
	} buddy_cfg;
};

long mm_initialize(struct mm_config* cfg, long shmm_key, int try_huge_page);

long mm_load(const char* mm_inf_file);
long mm_save(const char* mm_inf_file);
long mm_uninitialize(void);

void* mm_alloc(unsigned long size);
long mm_free(void* p);

#endif

