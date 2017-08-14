#include "mmspace.h"
#include "shmem.h"
#include "dlist.h"
#include "rbtree.h"
#include "mmpool.h"
#include "pgpool.h"

#include <string.h>
#include <stdlib.h>

struct _mm_space_impl
{
	struct shmm_blk* _shm;
	struct mmpool* _buddy_sys;
	struct pgpool* _pg_sys;

	struct mm_config _cfg;
};

static struct _mm_space_impl* __the_mmspace = 0;

long mm_initialize(struct mm_config* cfg, long shmm_key, int try_huge_page)
{
	if(__the_mmspace) goto error_ret;

	__the_mmspace = malloc(sizeof(struct _mm_space_impl));
	if(!__the_mmspace) goto error_ret;

	__the_mmspace->_shm = shmm_create_key(shmm_key, cfg->total_size, try_huge_page);
	if(!__the_mmspace->_shm) goto error_ret;

	__the_mmspace->_buddy_sys = mmp_create(__the_mmspace->_shm->addr_begin, cfg->buddy_cfg.size,
			cfg->buddy_cfg.min_block_order, cfg->buddy_cfg.max_block_order);
	if(!__the_mmspace->_buddy_sys) goto error_ret;

	__the_mmspace->_pg_sys = pgp_create(__the_mmspace->_buddy_sys->addr_end,
			__the_mmspace->_shm->addr_end - __the_mmspace->_buddy_sys->addr_end, cfg->maxpg_count);
	if(!__the_mmspace->_pg_sys) goto error_ret;

	memcpy(&__the_mmspace->_cfg, cfg, sizeof(struct mm_config));

	return 0;
error_ret:
	mm_uninitialize();
	return -1;
}

long mm_load(const char* mm_inf_file)
{
	return 0;
error_ret:
	return -1;
}

long mm_save(const char* mm_inf_file)
{
	return 0;
error_ret:
	return -1;
}

long mm_uninitialize(void)
{
	if(!__the_mmspace) goto error_ret;

	if(__the_mmspace->_pg_sys)
		pgp_destroy(__the_mmspace->_pg_sys);
	if(__the_mmspace->_buddy_sys)
		mmp_destroy(__the_mmspace->_buddy_sys);
	if(__the_mmspace->_shm)
		shmm_destroy(&__the_mmspace->_shm);

	free(__the_mmspace);
	return 0;
error_ret:
	return -1;
}

void* mm_alloc(unsigned long size)
{
	void* p;
	if(!__the_mmspace) goto error_ret;

	if(size <= (1 << __the_mmspace->_cfg.buddy_cfg.max_block_order))
		p = mmp_alloc(__the_mmspace->_buddy_sys, size);
	else
		p = pgp_alloc(__the_mmspace->_pg_sys, size);

	return p;
error_ret:
	return 0;
}

long mm_free(void* p)
{
	long rslt;

	if(!__the_mmspace) goto error_ret;

	if(p >= __the_mmspace->_buddy_sys->addr_begin && p <= __the_mmspace->_buddy_sys->addr_end)
		rslt = mmp_free(__the_mmspace->_buddy_sys, p);
	else if(p >= __the_mmspace->_pg_sys->addr_begin && p <= __the_mmspace->_pg_sys->addr_end)
		rslt = pgp_free(__the_mmspace->_pg_sys, p);
	else
		rslt = -1;

	return rslt;
error_ret:
	return -1;
}

