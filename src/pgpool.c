#include "pgpool.h"
#include "dlist.h"
#include "misc.h"
#include "rbtree.h"
#include <stdlib.h>


#define PG_SIZE			(4096)
#define PG_SIZE_SHIFT	(12)

#define PGP_CHUNK_LABEL (0xbaba2121fdfd9696)
#define HASH_COUNT		(256)

#pragma pack(1)

struct _chunk_header
{
	unsigned long _chunck_label;
	unsigned long _reserved;
};

#pragma pack()

struct _free_list_node
{
	union
	{
		struct dlnode _lst_node;
		struct dlnode _free_fln_node;
	};

	void* _payload_addr;
	long _pg_count;
};

struct _free_list_head
{
	struct dlist _free_list;
	long _op_count;
};

//struct _hash_list_node
//{
//	union
//	{
//		struct dlnode _lst_node;
//		struct dlnode _free_hln_node;
//	};
//	unsigned long _pg_addr;
//	long _pg_count;
//};

struct _rb_tree_node
{
	union
	{
		struct rbnode _rb_node;
		struct dlnode _free_rbl_node;
	};

	long _pg_count;
};

//struct _hash_list_head
//{
//	struct dlist _hash_list;
//	long _node_count;
//};
//

struct _pgp_cfg
{
	long maxpg_count;
	long freelist_count;
};

struct _pgpool_impl
{
	struct pgpool _the_pool;
	struct _pgp_cfg _cfg;

	void* _chunk_addr;
	long _chunk_pgcount;

	struct dlist _free_fln_list;
	struct _free_list_node* _fln_pool;
	struct _free_list_head* _flh;

	struct dlist _free_rbn_list;
	struct rbtree _allocated_tree;
	struct _rb_tree_node* _rbn_pool;

//	struct _hash_list_node* _hln_pool;
//	struct _hash_list_head _hlh[HASH_COUNT];
};


static inline unsigned long _align_pg(unsigned long addr)
{
	return (addr + (PG_SIZE - 1)) & (~(PG_SIZE - 1));
}

static inline long _hash_key(unsigned long addr)
{
	return (addr & 0xFF);
}

static inline struct _pgpool_impl* _conv_impl(struct pgpool* pgp)
{
	return (struct _pgpool_impl*)((void*)pgp - (unsigned long)(&((struct _pgpool_impl*)(0))->_the_pool));
}

static inline struct _free_list_node* _conv_fln(struct dlnode* dln)
{
	return (struct _free_list_node*)((void*)dln - (unsigned long)(&((struct _free_list_node*)(0))->_lst_node));
}

static inline struct _free_list_node* _conv_free_fln(struct dlnode* dln)
{
	return (struct _free_list_node*)((void*)dln - (unsigned long)(&((struct _free_list_node*)(0))->_free_fln_node));
}

static inline struct _rb_tree_node* _conv_rbn(struct rbnode* rbn)
{
	return (struct _rb_tree_node*)((void*)rbn - (unsigned long)(&((struct _rb_tree_node*)(0))->_rb_node));
}

static inline struct _rb_tree_node* _conv_free_rbn(struct dlnode* dln)
{
	return (struct _rb_tree_node*)((void*)dln - (unsigned long)(&((struct _rb_tree_node*)(0))->_free_rbl_node));
}

static inline struct _free_list_node* _fetch_free_fln(struct _pgpool_impl* pgpi)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_free_fln_list);
	struct _free_list_node* fln = _conv_free_fln(dln);

	return fln;
}

static inline struct _rb_tree_node* _fetch_free_rbn(struct _pgpool_impl* pgpi)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_free_rbn_list);
	struct _rb_tree_node* hln = _conv_free_rbn(dln);

	return hln;
}

static inline long _return_free_fln(struct _pgpool_impl* pgpi, struct _free_list_node* fln)
{
	fln->_payload_addr = 0;
	fln->_pg_count = 0;

	return lst_push_front(&pgpi->_free_fln_list, &fln->_free_fln_node);
}

static inline long _return_free_rtn(struct _pgpool_impl* pgpi, struct _rb_tree_node* rtn)
{
	return lst_push_front(&pgpi->_free_rbn_list, &rtn->_free_rbl_node);
}

static inline long _insert_rtn(struct _pgpool_impl* pgpi, void* pg_addr, long pg_count)
{
	long rslt = 0;
	unsigned long rb_key = (unsigned long)pg_addr;
	struct _rb_tree_node* rtn = _fetch_free_rbn(pgpi);

	rtn->_rb_node.key = rb_key;
	rtn->_pg_count = pg_count;

	rb_fillnew(&rtn->_rb_node);
	rslt = rb_insert(&pgpi->_allocated_tree, &rtn->_rb_node);
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}

static inline struct _free_list_node* _remove_rtn(struct _pgpool_impl* pgpi, void* pg_addr)
{
	long rslt = 0;
	unsigned long rb_key = (unsigned long)pg_addr;
	struct _rb_tree_node* rtn;
	struct _free_list_node* fln;

	struct rbnode* rbn = rb_remove(&pgpi->_allocated_tree, rb_key);
	if(!rbn) goto error_ret;

	rtn = _conv_rbn(rbn);

	fln = _fetch_free_fln(pgpi);
	if(!fln) goto error_ret;

	fln->_payload_addr = pg_addr;
	fln->_pg_count = rtn->_pg_count;

	rslt = _return_free_rtn(pgpi, rtn);
	if(rslt < 0) goto error_ret;

	return fln;
error_ret:
	return 0;
}

static inline struct _free_list_node* _fetch_fln(struct _pgpool_impl* pgpi, long flh_idx)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_flh[flh_idx]._free_list);
	return _conv_fln(dln);
}

static inline long _link_fln(struct _pgpool_impl* pgpi, struct _free_list_node* fln)
{
	long flh_idx = log_2(fln->_pg_count);
	return lst_push_front(&pgpi->_flh[flh_idx]._free_list, &fln->_lst_node);
}

static long _take_free_node(struct _pgpool_impl* pgpi, long pg_count, struct _free_list_node** fln)
{
	long rslt;
	long flh_idx = log_2(pg_count);
	long idx = flh_idx;

	struct dlnode* dln;
	struct _free_list_node* candi_fln = 0;

	*fln = 0;

	if(is_2power(pg_count) && !lst_empty(&pgpi->_flh[idx]._free_list))
	{
		*fln = _fetch_fln(pgpi, idx);
		if(!(*fln)) goto error_ret;

		goto succ_ret;
	}

	for(idx = flh_idx; idx < pgpi->_cfg.freelist_count; ++idx)
	{
		if(lst_empty(&pgpi->_flh[idx]._free_list))
			continue;

		dln = pgpi->_flh[idx]._free_list.head.next;

		while(dln != &pgpi->_flh[idx]._free_list.tail)
		{
			candi_fln = _conv_fln(dln);
			if(candi_fln->_pg_count >= pg_count)
			{
				lst_remove(&pgpi->_flh[idx]._free_list, dln);
				break;
			}
		}
	}

	if(!candi_fln) goto error_ret;

	*fln = candi_fln;

	if((candi_fln->_pg_count >> 1) >= pg_count)
	{
		struct _free_list_node* new_fln = _fetch_free_fln(pgpi);
		new_fln->_payload_addr = candi_fln->_payload_addr + pg_count * PG_SIZE;
		new_fln->_pg_count = candi_fln->_pg_count - pg_count;

		_link_fln(pgpi, new_fln);
		candi_fln->_pg_count -= pg_count;
	}

succ_ret:
	return 0;
error_ret:
	return -1;
}

static long _pgp_init_chunk(struct _pgpool_impl* pgpi)
{
	struct _chunk_header* hd;
	long remain_count, flh_idx, rslt;
	void* pg;

	hd = pgpi->_chunk_addr - sizeof(struct _chunk_header);
	remain_count = pgpi->_chunk_pgcount - pgpi->_cfg.maxpg_count;
	pg = pgpi->_chunk_addr;

	while(remain_count > 0)
	{
		struct dlnode* dln = lst_pop_front(&pgpi->_free_fln_list);
		struct _free_list_node* fln = _conv_fln(dln);

		fln->_payload_addr = pg;
		if(remain_count >= pgpi->_cfg.maxpg_count)
			fln->_pg_count = pgpi->_cfg.maxpg_count;
		else
			fln->_pg_count = remain_count;

		flh_idx = log_2(fln->_pg_count);

		lst_clr(&fln->_lst_node);
		rslt = lst_push_back(&pgpi->_flh[flh_idx]._free_list, &fln->_lst_node);

		if(rslt < 0) goto error_ret;

		pg += pgpi->_cfg.maxpg_count;
		remain_count -= pgpi->_cfg.maxpg_count;
	}

	hd->_chunck_label = PGP_CHUNK_LABEL;

	return 0;
error_ret:
	return -1;
}

static long _pgp_load_chunk(struct _pgpool_impl* pgpi)
{

	return 0;
error_ret:
	return -1;
}

struct pgpool* pgp_new(void* addr, long size, struct pgpool_config* cfg)
{
	long rslt = 0;
	struct _chunk_header* hd;
	struct _pgpool_impl* pgpi;
	if(!addr || size <= PG_SIZE) goto error_ret;

	pgpi = (struct _pgpool_impl*)malloc(sizeof(struct _pgpool_impl));
	if(!pgpi) goto error_ret;

	lst_new(&pgpi->_free_fln_list);
	lst_new(&pgpi->_free_rbn_list);

	pgpi->_the_pool.addr = addr;
	pgpi->_the_pool.size = size;
	pgpi->_cfg.maxpg_count = cfg->maxpg_count;
	pgpi->_cfg.freelist_count = log_2(cfg->maxpg_count) + 1;

	pgpi->_chunk_addr = (void*)_align_pg((unsigned long)addr + sizeof(struct _chunk_header));
	pgpi->_chunk_pgcount = (size - (pgpi->_chunk_addr - addr)) / PG_SIZE;

	pgpi->_fln_pool = malloc(sizeof(struct _free_list_node) * pgpi->_chunk_pgcount);
	if(!pgpi->_fln_pool) goto error_ret;

	pgpi->_rbn_pool = malloc(sizeof(struct _rb_tree_node) * pgpi->_chunk_pgcount);
	if(!pgpi->_rbn_pool) goto error_ret;

	pgpi->_flh = malloc(sizeof(struct _free_list_head) * pgpi->_cfg.freelist_count);
	if(!pgpi->_flh) goto error_ret;

	for(long i = 0; i < pgpi->_chunk_pgcount; ++i)
	{
		lst_clr(&pgpi->_fln_pool[i]._lst_node);
		lst_push_back(&pgpi->_free_fln_list, &pgpi->_fln_pool[i]._free_fln_node);

		lst_clr(&pgpi->_rbn_pool[i]._free_rbl_node);
		lst_push_back(&pgpi->_free_rbn_list, &pgpi->_rbn_pool[i]._free_rbl_node);
	}

	hd = pgpi->_chunk_addr - sizeof(struct _chunk_header);
	if(hd->_chunck_label != PGP_CHUNK_LABEL)
		rslt = _pgp_init_chunk(pgpi);
	else
		rslt = _pgp_load_chunk(pgpi);

	if(rslt < 0) goto error_ret;

	return &pgpi->_the_pool;
error_ret:
	if(pgpi)
		pgp_del(&pgpi->_the_pool);

	return 0;
}

void pgp_del(struct pgpool* pgp)
{
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	if(pgpi)
	{
		if(pgpi->_flh)
			free(pgpi->_flh);
		if(pgpi->_rbn_pool)
			free(pgpi->_rbn_pool);
		if(pgpi->_fln_pool)
			free(pgpi->_fln_pool);

		free(pgpi);
	}
}


void* pgp_alloc(struct pgpool* pgp, long size)
{
	void* payload;
	long flh_idx, rslt, pg_count;
	struct _free_list_node* fln;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	pg_count = (size + PG_SIZE - 1) >> PG_SIZE_SHIFT;

	rslt = _take_free_node(pgpi, pg_count, &fln);
	if(rslt < 0 || !fln) goto error_ret;

	rslt = _insert_rtn(pgpi, fln->_payload_addr, fln->_pg_count);
	if(rslt < 0) goto error_ret;

	payload = fln->_payload_addr;

	_return_free_fln(pgpi, fln);

	return payload;
error_ret:
	return 0;
}

long pgp_free(struct pgpool* pgp, void* p)
{
	long rslt;

	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	struct _free_list_node* fln = _remove_rtn(pgpi, p);
	if(!fln) goto error_ret;

	rslt = _link_fln(pgpi, fln);
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}



