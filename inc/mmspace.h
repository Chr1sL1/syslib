#ifndef __mmspace_h__
#define __mmspace_h__

#include "mmops.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmzone: object cache.
// struct mmzone its self is allocated from MM_AREA_PERSIS area,
// while cached objects are allocated from MM_AREA_ZONE area.

struct mmzone
{
	unsigned int obj_size;
	unsigned int padding;
};

struct mmzone* mm_zcreate(const char* name, unsigned int obj_size);
long mm_zdestroy(struct mmzone* mmz);

void* mm_zalloc(struct mmzone* mmz);
long mm_zfree(struct mmzone* mmz, void* p);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// mmspace: a group of shared memory in which the whole process's data lays.

enum MM_AREA_TYPE
{
	MM_AREA_BEGIN = 0,

	MM_AREA_NUBBLE = MM_AREA_BEGIN,		//< small memory block
	MM_AREA_PAGE,						//< page-aligned memory block
	MM_AREA_ZONE,						//< for mmzone object
	MM_AREA_PERSIS,						//< for persistent data

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

void mm_save_globl_data(void* p);
void* mm_load_globl_data(void);

struct mmzone* mm_search_zone(const char* zone_name);

void* mm_alloc(unsigned long size);
void* mm_area_alloc(unsigned long size, int area_type);

long mm_free(void* p);

#endif

