#include "mmspace.h"
#include "shmem.h"
#include "dlist.h"
#include "rbtree.h"
#include "misc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define MM_LABEL (0x6666666666666666UL)
#define SLAB_LABEL (0x8765432101234567UL)

struct _mm_section_impl
{
	int _area_type;
	int _padding;

	void* _allocator;

}__attribute__((aligned(8)));

struct _mm_area_impl
{
	struct dlist _section_list;
	struct _mm_section_impl* _free_section;

}__attribute__((aligned(8)));


struct _mm_shmm_save
{
	void* _base_addr;
	int _key;
	int _size;

}__attribute__((aligned(8)));

struct _mm_space_impl
{
	unsigned long _mm_label;
	int _next_shmm_key;
	int _total_shmm_count;

	struct mm_space_config _cfg;
	struct _mm_area_impl _area_list[MM_AREA_COUNT];

	struct rbtree _all_section_tree;

	struct _mm_shmm_save* _shmm_save_list;

}__attribute__((aligned(8)));

struct _mmzone_impl
{
	struct mmzone _the_zone;
	struct dlist _full_slab_list;
	struct dlist _empty_slab_list;
	struct dlist _partial_slab_list;
};

static struct _mm_space_impl* __the_mmspace = 0;
unsigned long __mm_page_size = 0x1000;

extern struct mm_ops __mmp_ops;
extern struct mm_ops __pgp_ops;

static struct mm_ops* __mm_area_ops[MM_AREA_COUNT] =
{
	[MM_AREA_NUBBLE_ALLOC] = &__mmp_ops,
	[MM_AREA_PAGE_ALLOC] = &__pgp_ops,
	[MM_AREA_ZONE_ALLOC] = &__pgp_ops,
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// mmzone:
//

struct _mm_cache
{
	unsigned long _slab_label;
	struct _mmzone_impl* _zone;
	struct dlnode _list_node;

	unsigned int _free_idx;
	unsigned int _obj_count;

	void* _obj_ptr;
};

static inline struct _mm_cache* _cache_of_obj(void* obj)
{
	struct _mm_cache* s = (struct _mm_cache*)((unsigned long)obj & ~(__mm_page_size - 1));

	while(s != 0 && s->_slab_label != SLAB_LABEL)
		s = (struct _mm_cache*)((void*)s - __mm_page_size);

	return s;
}

struct mmzone* mm_zcreate(unsigned long obj_size)
{

error_ret:
	return 0;
}

long mm_zdestroy(struct mmzone* mmz)
{
error_ret:
	return -1;
}

void* mm_zalloc(struct mmzone* mmz)
{

error_ret:
	return 0;
}

long mm_zfree(struct mmzone* mmz, void* p)
{

error_ret:
	return -1;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// mmspace:
//

static inline int _make_shmm_key(long ar_type, long area_idx)
{
	long channel_id = ((ar_type + 1) << 4) + area_idx;

	return channel_id;
//	return ftok("/dev/null", channel_id);
}

static inline struct shmm_blk* _conv_shmm_from_rbn(struct rbnode* rbn)
{
	return (struct shmm_blk*)((unsigned long)rbn - (unsigned long)&((struct shmm_blk*)(0))->rb_node);
}

static inline struct shmm_blk* _conv_shmm_from_dln(struct dlnode* dln)
{
	return (struct shmm_blk*)((unsigned long)dln - (unsigned long)&((struct shmm_blk*)(0))->lst_node);
}

static inline struct _mm_section_impl* _get_section(struct shmm_blk* shm)
{
	return (struct _mm_section_impl*)shm->addr_begin;
}

static long _shmm_comp_func(void* key, struct rbnode* rbn)
{
	struct shmm_blk* shm = _conv_shmm_from_rbn(rbn);
	if(key < shm->addr_begin) return -1;
	else if(key >= shm->addr_end) return 1;

	return 0;
}

static long _mm_load_area(struct _mm_space_impl* mm)
{
	long rslt;
	struct dlnode* dln;

	for(int i = 0; i < mm->_total_shmm_count; ++i)
	{
		printf("load section shmm_key: 0x%x\n", mm->_shmm_save_list[i]._key);
		struct shmm_blk* shm = shmm_open(mm->_shmm_save_list[i]._key, mm->_shmm_save_list[i]._base_addr);
		if(!shm) goto error_ret;
	}

	for(int i = MM_AREA_BEGIN; i < MM_AREA_COUNT; ++i)
	{
		dln = mm->_area_list[i]._section_list.head.next;

		while(dln != &mm->_area_list[i]._section_list.tail)
		{
			struct shmm_blk* shm = _conv_shmm_from_dln(dln);

			struct _mm_section_impl* sec = _get_section(shm);

			sec->_allocator = (*__mm_area_ops[i]->load_func)(sec->_allocator);
			if(!sec->_allocator) goto error_ret;

			dln = dln->next;
		}
	}

	return 0;
error_ret:
	printf("_mm_load_area failed.\n");
	return -1;
}

static long _mm_create_section(struct _mm_space_impl* mm, int ar_type)
{
	long rslt;
	int shmm_key;
	struct _mm_area_impl* ar;
	struct _mm_section_impl* sec = 0;
	struct shmm_blk* shm;

	shmm_key = _make_shmm_key(ar_type, ++mm->_next_shmm_key);

	shm = shmm_create(shmm_key, 0, mm->_cfg.mm_cfg[ar_type].total_size, mm->_cfg.try_huge_page);
	if(!shm) goto error_ret;

	ar = &mm->_area_list[ar_type];

	lst_new(&ar->_section_list);
	
	rb_fillnew(&shm->rb_node);
	rslt = rb_insert(&mm->_all_section_tree, &shm->rb_node);
	if(rslt < 0) goto error_ret;

	lst_clr(&shm->lst_node);
	rslt = lst_push_back(&ar->_section_list, &shm->lst_node);
	if(rslt < 0) goto error_ret;

	sec = (struct _mm_section_impl*)(shm->addr_begin);

	sec->_area_type = ar_type;
	sec->_padding = 0x12345678;

	sec->_allocator = (*__mm_area_ops[ar_type]->create_func)(shm->addr_begin + sizeof(struct _mm_section_impl), &mm->_cfg.mm_cfg[ar_type]);
	if(!sec->_allocator) goto error_ret;

	ar->_free_section = sec;

	mm->_shmm_save_list[mm->_total_shmm_count]._base_addr = shm;
	mm->_shmm_save_list[mm->_total_shmm_count]._key = shmm_key;
	mm->_shmm_save_list[mm->_total_shmm_count]._size = mm->_cfg.mm_cfg[ar_type].total_size;

	++mm->_total_shmm_count;

	printf("new section shmm_key: 0x%x, size: %lu\n", shmm_key, mm->_cfg.mm_cfg[ar_type].total_size);

	return 0;
error_ret:
	printf("_mm_creat_section failed.\n");
	if(shm)
		shmm_destroy(shm);
	return -1;

}

long mm_initialize(struct mm_space_config* cfg)
{
	long rslt;
	unsigned long shm_size;
	struct shmm_blk* shm;
	struct _mm_space_impl* mm;

	if(__the_mmspace || !cfg || cfg->max_shmm_count <= 0) goto error_ret;

	shm_size = sizeof(struct _mm_space_impl) + sizeof(struct _mm_shmm_save) * cfg->max_shmm_count;

	shm = shmm_open_raw(cfg->sys_shmm_key, (void*)cfg->sys_begin_addr);
	if(shm)
	{
		mm = (struct _mm_space_impl*)shm->addr_begin;
		if(mm->_mm_label != MM_LABEL) goto error_ret;

		rslt = _mm_load_area(mm);
		if(rslt < 0) goto error_ret;

		rb_reset_compare_function(&mm->_all_section_tree, _shmm_comp_func);

		goto succ_ret;
	}

	shm = shmm_create(cfg->sys_shmm_key, (void*)cfg->sys_begin_addr, shm_size, cfg->try_huge_page);
	if(!shm) goto error_ret;

	mm = (struct _mm_space_impl*)(shm->addr_begin);
	memcpy(&mm->_cfg, cfg, sizeof(struct mm_space_config));
	mm->_mm_label = MM_LABEL;

	rb_init(&mm->_all_section_tree, _shmm_comp_func);

	mm->_total_shmm_count = 0;
	mm->_shmm_save_list = (struct _mm_shmm_save*)((void*)mm + sizeof(struct _mm_space_impl));

	for(int i = MM_AREA_BEGIN; i < MM_AREA_COUNT; ++i)
	{
		rslt = _mm_create_section(mm, i);
		if(rslt < 0) goto error_ret;
	}

succ_ret:
	__the_mmspace = mm;
	return 0;
error_ret:
	mm_uninitialize();
	return -1;
}

long mm_uninitialize(void)
{
	if(!__the_mmspace) goto error_ret;

	return 0;
error_ret:
	return -1;
}

static void* _mm_area_alloc(unsigned long size, int ar_type)
{
	long rslt;
	void* p;
	struct dlnode* dln;
	struct shmm_blk* shm;
	struct _mm_area_impl* ar;

	ar = &__the_mmspace->_area_list[ar_type];

	if(!ar->_free_section) goto error_ret;

retry_alloc:

	p = (*__mm_area_ops[ar_type]->alloc_func)(ar->_free_section->_allocator, size);
	if(p) goto succ_ret;

	dln = ar->_section_list.head.next;

	while(dln != &ar->_section_list.tail)
	{
		shm = _conv_shmm_from_dln(dln);
		ar->_free_section = _get_section(shm);

		p = (*__mm_area_ops[ar_type]->alloc_func)(ar->_free_section->_allocator, size);
		if(p) goto succ_ret;

		dln = dln->next;
	}

	rslt = _mm_create_section(__the_mmspace, ar_type);
	if(rslt < 0) goto error_ret;

	goto retry_alloc;

succ_ret:
	return p;
error_ret:
	return 0;

}

void* mm_alloc(unsigned long size)
{
	unsigned long odr;
	int ar_type;

	if(size == 0) goto error_ret;
	if(!__the_mmspace) goto error_ret;

	odr = log_2(size) + 1;

	if(odr < __the_mmspace->_cfg.mm_cfg[MM_AREA_NUBBLE_ALLOC].max_order)
		ar_type = MM_AREA_NUBBLE_ALLOC;
	else
		ar_type = MM_AREA_PAGE_ALLOC;

	return _mm_area_alloc(size, ar_type);

error_ret:
	return 0;
}

long mm_free(void* p)
{
	long rslt;
	struct rbnode* rbn, *hot;
	struct _mm_section_impl* sec;
	struct shmm_blk* shm;

	if(!__the_mmspace) goto error_ret;

	rbn = rb_search(&__the_mmspace->_all_section_tree, p, &hot);
	if(!rbn) goto error_ret;

	shm = _conv_shmm_from_rbn(rbn);
	sec = _get_section(shm);

	rslt = (*__mm_area_ops[sec->_area_type]->free_func)(sec->_allocator, p);
	if(!rslt) goto error_ret;

	return rslt;
error_ret:
	return -1;
}

