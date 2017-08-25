#ifndef __mmspace_h__
#define __mmspace_h__

#include "mmops.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmzone: object cache.

struct mmzone
{
	unsigned long obj_size;
};

struct mmzone* mm_zcreate(unsigned long obj_size);
long mm_zdestroy(struct mmzone* mmz);

void* mm_zalloc(struct mmzone* mmz);
long mm_zfree(struct mmzone* mmz, void* p);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmspace: a huge block of shared memory in which the whole process's data lays
//
//
//

enum MM_AREA_TYPE
{
	MM_AREA_BEGIN = 0,

	MM_AREA_NUBBLE_ALLOC = MM_AREA_BEGIN,
	MM_AREA_PAGE_ALLOC,
	MM_AREA_ZONE_ALLOC,

	MM_AREA_COUNT,
};

struct mm_space_config
{
	unsigned long sys_begin_addr;
	int sys_shmm_key;
	int try_huge_page;
	int max_shmm_count;

	struct mm_config mm_cfg[MM_AREA_COUNT];
};

long mm_initialize(struct mm_space_config* cfg);
long mm_uninitialize(void);

void* mm_alloc(unsigned long size);
long mm_free(void* p);

#endif

