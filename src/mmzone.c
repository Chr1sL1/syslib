#include "mmzone.h"
#include "dlist.h"
#include "rbtree.h"
#include "mmpool.h"
#include "pgpool.h"
#include "uma.h"
#include "misc.h"

#define MMZONE_LABEL (0x7a6f6e65)

#pragma pack(1)

struct _mm_zone_header
{
	unsigned int _zone_label;
	int _zone_type;
	unsigned long _addr_begin;
	unsigned long _addr_end;
};

#pragma pack()

struct _mm_zone_impl
{
	struct mm_zone _the_zone;
	void* _chuck_addr;

	struct dlnode _list_node;
	struct rbnode _rb_node;

	union
	{
		void* _alloc_shadow;
		struct mmpool* _alloc_mmpool;
		struct pgpool* _alloc_pgpool;
		struct uma* _alloc_uma;
	};
};

typedef void* (*_alloc_create_func)(struct _mm_zone_impl*, struct mm_zone_config*);
typedef void* (*_alloc_load_func)(struct _mm_zone_impl*);

typedef void* (*_alloc_alloc_func)(struct _mm_zone_impl*, unsigned long);
typedef long (*_alloc_free_func)(struct _mm_zone_impl*, void*);

static void* _alloc_mmp_create(struct _mm_zone_impl* mzi, struct mm_zone_config* cfg)
{
	return mmp_create(mzi->_chuck_addr, mzi->_the_zone.addr_end - mzi->_chuck_addr,
			cfg->mmp_cfg.min_block_order, cfg->mmp_cfg.max_block_order);
}

static void* _alloc_pgp_create(struct _mm_zone_impl* mzi, struct mm_zone_config* cfg)
{
	return pgp_create(mzi->_chuck_addr, mzi->_the_zone.addr_end - mzi->_chuck_addr,
			cfg->pgp_cfg.maxpg_count);
}

static void* _alloc_uma_create(struct _mm_zone_impl* mzi, struct mm_zone_config* cfg)
{
	return uma_create(mzi->_chuck_addr, mzi->_the_zone.addr_end - mzi->_chuck_addr,
			cfg->uma_cfg.obj_size);
}

static void* _alloc_mmp_load(struct _mm_zone_impl* mzi)
{
	return mmp_load(mzi->_chuck_addr);
}

static void* _alloc_pgp_load(struct _mm_zone_impl* mzi)
{
	return pgp_load(mzi->_chuck_addr);
}

static void* _alloc_uma_load(struct _mm_zone_impl* mzi)
{
	return uma_load(mzi->_chuck_addr);
}

static void* _alloc_mmp_alloc(struct _mm_zone_impl* mzi, unsigned long size)
{
	return mmp_alloc(mzi->_alloc_mmpool, size);
}

static void* _alloc_pgp_alloc(struct _mm_zone_impl* mzi, unsigned long size)
{
	return pgp_alloc(mzi->_alloc_pgpool, size);
}

static void* _alloc_uma_alloc(struct _mm_zone_impl* mzi, unsigned long size)
{
	return uma_alloc(mzi->_alloc_uma);
}

static long _alloc_mmp_free(struct _mm_zone_impl* mzi, void* p)
{
	return mmp_free(mzi->_alloc_mmpool, p);
}

static long _alloc_pgp_free(struct _mm_zone_impl* mzi, void* p)
{
	return pgp_free(mzi->_alloc_pgpool, p);
}

static long _alloc_uma_free(struct _mm_zone_impl* mzi, void* p)
{
	return uma_free(mzi->_alloc_uma, p);
}
static _alloc_create_func __alloc_create_func[] =
{
	_alloc_mmp_create,
	_alloc_pgp_create,
	_alloc_uma_create,
};

static _alloc_load_func __alloc_load_func[] =
{
	_alloc_mmp_load,
	_alloc_pgp_load,
	_alloc_uma_load,
};

static _alloc_alloc_func __alloc_alloc_func[] =
{
	_alloc_mmp_alloc,
	_alloc_pgp_alloc,
	_alloc_uma_alloc,
};

static _alloc_free_func __alloc_free_func[] =
{
	_alloc_mmp_free,
	_alloc_pgp_free,
	_alloc_uma_free,
};

static inline struct _mm_zone_impl* _conv_impl(struct mm_zone* mmz)
{
	return (struct _mm_zone_impl*)((unsigned long)mmz - (unsigned long)(&(((struct _mm_zone_impl*)0)->_the_zone)));
}

static inline struct _mm_zone_impl* _conv_lstnode(struct dlnode* dln)
{
	return (struct _mm_zone_impl*)((unsigned long)dln - (unsigned long)(&(((struct _mm_zone_impl*)0)->_list_node)));
}

static inline struct _mm_zone_impl* _conv_rbnode(struct rbnode* rbn)
{
	return (struct _mm_zone_impl*)((unsigned long)rbn - (unsigned long)(&(((struct _mm_zone_impl*)0)->_rb_node)));
}

struct mm_zone* mmz_create(long type, void* addr_begin, void* addr_end, struct mm_zone_config* cfg)
{
	struct _mm_zone_header* hd;
	struct _mm_zone_impl* mzi;
	void* cur_pos = addr_begin;

	if(!addr_begin || !addr_end) goto error_ret;
	if(addr_begin >= addr_end) goto error_ret;
	if(((unsigned long)addr_begin & 0x7) != 0 || ((unsigned long)addr_end & 0x7) != 0) goto error_ret;
	if(addr_end - addr_begin <= sizeof(struct _mm_zone_header) + sizeof(struct _mm_zone_impl)) goto error_ret;
	if(type <= MMZ_INVALID || type >= MMZ_COUNT) goto error_ret;

	hd = (struct _mm_zone_header*)cur_pos;
	cur_pos = move_ptr_align8(cur_pos, sizeof(struct _mm_zone_header));

	hd->_zone_label = MMZONE_LABEL;
	hd->_zone_type = type;
	hd->_addr_begin = (unsigned long)addr_begin;
	hd->_addr_end = (unsigned long)addr_end;

	mzi = (struct _mm_zone_impl*)cur_pos;
	cur_pos = move_ptr_align8(cur_pos, sizeof(struct _mm_zone_impl));

	mzi->_chuck_addr = cur_pos;
	mzi->_the_zone.type = type;
	mzi->_the_zone.addr_begin = addr_begin;
	mzi->_the_zone.addr_end = addr_end;

	lst_clr(&mzi->_list_node);
	rb_fillnew(&mzi->_rb_node);

	mzi->_alloc_shadow = (*__alloc_create_func[mzi->_the_zone.type])(mzi, cfg);
	if(!mzi->_alloc_shadow) goto error_ret;

	return &mzi->_the_zone;
error_ret:
	return 0;
}

struct mm_zone* mmz_load(void* addr)
{
	struct _mm_zone_header* hd;
	struct _mm_zone_impl* mzi;
	void* cur_pos = addr;

	if(!addr) goto error_ret;
	if(((unsigned long)addr & 0x7) != 0) goto error_ret;

	hd = (struct _mm_zone_header*)cur_pos;
	cur_pos = move_ptr_align8(cur_pos, sizeof(struct _mm_zone_header));

	if(hd->_zone_label != MMZONE_LABEL) goto error_ret;
	if(hd->_zone_type <= MMZ_INVALID || hd->_zone_type >= MMZ_COUNT) goto error_ret;
	if(hd->_addr_begin != (unsigned long)addr) goto error_ret;

	mzi = (struct _mm_zone_impl*)cur_pos;
	if(mzi->_the_zone.addr_begin != addr || (unsigned long)mzi->_the_zone.addr_end != hd->_addr_end)
		goto error_ret;

	if(mzi->_the_zone.type != hd->_zone_type)
		goto error_ret;

	mzi->_alloc_shadow = (*__alloc_load_func[mzi->_the_zone.type])(mzi);
	if(!mzi->_alloc_shadow) goto error_ret;

	return &mzi->_the_zone;
error_ret:
	return 0;
}

void mmz_destroy(struct mm_zone* mmz)
{
	struct _mm_zone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

error_ret:
	return;
}

void* mmz_alloc(struct mm_zone* mmz, unsigned long size)
{
	struct _mm_zone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	if(mzi->_the_zone.type <= MMZ_INVALID || mzi->_the_zone.type >= MMZ_COUNT)
		goto error_ret;

	return (*__alloc_alloc_func[mzi->_the_zone.type])(mzi, size);
error_ret:
	return 0;
}

long mmz_free(struct mm_zone* mmz, void* p)
{
	struct _mm_zone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	if(mzi->_the_zone.type <= MMZ_INVALID || mzi->_the_zone.type >= MMZ_COUNT)
		goto error_ret;

	return (*__alloc_free_func[mzi->_the_zone.type])(mzi, p);
error_ret:
	return -1;
}


inline struct dlnode* mmz_lstnode(struct mm_zone* mmz)
{
	struct _mm_zone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	return &mzi->_list_node;
error_ret:
	return 0;
}

inline struct rbnode* mmz_rbnode(struct mm_zone* mmz)
{
	struct _mm_zone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	return &mzi->_rb_node;
error_ret:
	return 0;
}

long _comp_zone_node(void* key, struct rbnode* n)
{
	struct _mm_zone_impl* mzi = _conv_rbnode(n);

	if(key < mzi->_the_zone.addr_begin)
		return -1;
	else if(key < mzi->_the_zone.addr_end)
		return 0;

	return 1;
}




