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

	struct dlist _zone_list;
};

struct _mmzone_impl
{
	struct mmzone _the_zone;
	struct dlist _free_list;
	struct dlnode _list_node_in_space;
};

static struct _mm_space_impl* __the_mmspace = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// mmzone:
//
struct _zone_free_list_node
{
	struct dlnode _fln;
	void* p;
};

static inline struct _mmzone_impl* _conv_impl(struct mmzone* mmz)
{
	return (struct _mmzone_impl*)((unsigned long)mmz - (unsigned long)(&((struct _mmzone_impl*)(0))->_the_zone));
}

static inline struct _zone_free_list_node* _conv_fln(struct dlnode* dln)
{
	return (struct _zone_free_list_node*)((unsigned long)dln- (unsigned long)(&((struct _zone_free_list_node*)(0))->_fln));
}

static inline long _link_zone(struct _mmzone_impl* mzi)
{
	lst_clr(&mzi->_list_node_in_space);
	return lst_push_back(&__the_mmspace->_zone_list, &mzi->_list_node_in_space);
}

static inline long _unlink_zone(struct _mmzone_impl* mzi)
{
	return lst_remove(&__the_mmspace->_zone_list, &mzi->_list_node_in_space);
}

static void _zone_return_all_free_obj(struct _mmzone_impl* mzi)
{
	struct dlnode* dln = mzi->_free_list.head.next;
	while(dln != &mzi->_free_list.tail)
	{
		struct _zone_free_list_node* fln = _conv_fln(dln);
		mm_free(fln->p);
		mm_free(fln);
		dln = dln->next;
	}
}

struct mmzone* mm_zcreate(unsigned long obj_size)
{
	struct _mmzone_impl* mzi;
	if(obj_size == 0) goto error_ret;
	if(!__the_mmspace) goto error_ret;

	mzi = malloc(sizeof(struct _mmzone_impl));
	if(!mzi) goto error_ret;

	mzi->_the_zone.obj_size = obj_size;
	mzi->_the_zone.current_free_count = 0;

	lst_new(&mzi->_free_list);

	_link_zone(mzi);

	return &mzi->_the_zone;
error_ret:
	return 0;
}

long mm_zdestroy(struct mmzone* mmz)
{
	struct dlnode* dln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	_unlink_zone(mzi);

	//return all freenode and data to mmspace.
	//
	//

	dln = &mzi->_free_list.head;
	while(dln != &mzi->_free_list.tail)
	{
		struct _zone_free_list_node* fln = _conv_fln(dln);
		mm_free(fln->p);
		mm_free(fln);

		dln = dln->next;
	}

	//

	free(mzi);

	return 0;
error_ret:
	return -1;
}

void* mm_zalloc(struct mmzone* mmz)
{
	void* p;
	struct dlnode* dln;
	struct _zone_free_list_node* fln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	if(!lst_empty(&mzi->_free_list))
	{
		dln = lst_pop_front(&mzi->_free_list);
		fln = _conv_fln(dln);
		p = fln->p;
		mm_free(fln);

		--mzi->_the_zone.current_free_count;
	}
	else
		p = mm_alloc(mzi->_the_zone.obj_size);

	return p;
error_ret:
	return 0;
}

long mm_zfree(struct mmzone* mmz, void* p)
{
	long rslt;
	struct _zone_free_list_node* fln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	fln = mm_alloc(sizeof(struct _zone_free_list_node));
	if(!fln) goto error_ret;

	fln->p = p;
	rslt = lst_push_front(&mzi->_free_list, &fln->_fln);
	if(!rslt) goto error_ret;

	++mzi->_the_zone.current_free_count;

	return 0;
error_ret:
	return -1;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// mmspace:

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


