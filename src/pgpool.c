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

struct _pg_node
{
	union
	{
		struct dlnode _free_pgn_node;
		struct dlnode _fln_node;
	};

	struct rbnode _rb_node;

	union
	{
		void* _payload_addr;
		unsigned using : 2;
	};

	long _pg_count;
};

struct _free_list_head
{
	struct dlist _free_list;
	long _op_count;
};

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

	struct rbtree _pgn_tree;
	struct _free_list_head* _flh;

	struct dlist _free_pgn_list;
	struct _pg_node* _pgn_pool;
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

static inline struct _pg_node* _conv_rbn(struct rbnode* rbn)
{
	return (struct _pg_node*)((void*)rbn - (unsigned long)(&((struct _pg_node*)(0))->_rb_node));
}

static inline struct _pg_node* _conv_fln(struct dlnode* fln)
{
	return (struct _pg_node*)((void*)fln - (unsigned long)(&((struct _pg_node*)(0))->_fln_node));
}

static inline struct _pg_node* _conv_free_pgn(struct dlnode* fln)
{
	return (struct _pg_node*)((void*)fln - (unsigned long)(&((struct _pg_node*)(0))->_free_pgn_node));
}

static inline struct _pg_node* _fetch_free_pgn(struct _pgpool_impl* pgpi)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_free_pgn_list);
	struct _pg_node* pgn = _conv_free_pgn(dln);

	return pgn;
}

static inline long _return_free_pgn(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	return lst_push_front(&pgpi->_free_pgn_list, &pgn->_free_pgn_node);
}

static inline struct _pg_node* _fetch_fln(struct _pgpool_impl* pgpi, long flh_idx)
{
	struct dlnode* fln = lst_pop_front(&pgpi->_flh[flh_idx]._free_list);
	return _conv_fln(fln);
}

static inline long _link_fln(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long flh_idx = log_2(pgn->_pg_count);
	return lst_push_front(&pgpi->_flh[flh_idx]._free_list, &pgn->_fln_node);
}

static inline long _unlink_fln(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long flh_idx = log_2(pgn->_pg_count);
	return lst_remove(&pgpi->_flh[flh_idx]._free_list, &pgn->_fln_node);
}

static inline long _link_rbn(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	rb_fillnew(&pgn->_rb_node);
	pgn->_rb_node.key = (unsigned long)pgn->_payload_addr;

	return rb_insert(&pgpi->_pgn_tree, &pgn->_rb_node);
}

static inline void _unlink_rbn(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	rb_remove_node(&pgpi->_pgn_tree, &pgn->_rb_node);
}

static inline struct _pg_node* _pgn_from_payload(struct _pgpool_impl* pgpi, void* payload)
{
	struct rbnode* hot;
	struct rbnode* rbn;

	rbn = rb_search(&pgpi->_pgn_tree, (unsigned long)payload, &hot);
	if(!rbn) goto error_ret;

	return _conv_rbn(rbn);
error_ret:
	return 0;
}

static long _take_free_node(struct _pgpool_impl* pgpi, long pg_count, struct _pg_node** pgn)
{
	long rslt;
	long flh_idx = log_2(pg_count);
	long idx = flh_idx;

	struct dlnode* dln;
	struct _pg_node* candi_pgn = 0;

	*pgn = 0;

	if(is_2power(pg_count) && !lst_empty(&pgpi->_flh[idx]._free_list))
	{
		*pgn = _fetch_fln(pgpi, idx);
		if(!(*pgn) || (*pgn)->using) goto error_ret;

		goto succ_ret;
	}

	for(idx = flh_idx; idx < pgpi->_cfg.freelist_count; ++idx)
	{
		if(lst_empty(&pgpi->_flh[idx]._free_list))
			continue;

		dln = pgpi->_flh[idx]._free_list.head.next;

		while(dln != &pgpi->_flh[idx]._free_list.tail)
		{
			candi_pgn = _conv_fln(dln);
			if(candi_pgn->_pg_count >= pg_count)
			{
				if(candi_pgn->using) goto error_ret;
				_unlink_fln(pgpi, candi_pgn);
				break;
			}
		}
	}

	if(!candi_pgn) goto error_ret;

	*pgn = candi_pgn;

	if(candi_pgn->_pg_count > pg_count)
	{
		struct _pg_node* new_pgn = _fetch_free_pgn(pgpi);
		new_pgn->_payload_addr = candi_pgn->_payload_addr + pg_count * PG_SIZE;
		new_pgn->_pg_count = candi_pgn->_pg_count - pg_count;
		candi_pgn->_pg_count -= pg_count;

		_link_fln(pgpi, new_pgn);
		_link_rbn(pgpi, new_pgn);
	}

succ_ret:
	return 0;
error_ret:
	return -1;
}

static long _return_free_node(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long rslt;
	struct rbnode* parent;

	parent = rb_parent(&pgn->_rb_node);
	if(parent)
	{
		struct _pg_node* parent_pgn = _conv_rbn(parent);

	}

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
		struct _pg_node* pgn = _fetch_free_pgn(pgpi);

		pgn->_payload_addr = pg;
		if(remain_count >= pgpi->_cfg.maxpg_count)
			pgn->_pg_count = pgpi->_cfg.maxpg_count;
		else
			pgn->_pg_count = remain_count;

		_link_fln(pgpi, pgn);
		_link_rbn(pgpi, pgn);

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

	lst_new(&pgpi->_free_pgn_list);

	pgpi->_the_pool.addr = addr;
	pgpi->_the_pool.size = size;
	pgpi->_cfg.maxpg_count = cfg->maxpg_count;
	pgpi->_cfg.freelist_count = log_2(cfg->maxpg_count) + 1;

	pgpi->_chunk_addr = (void*)_align_pg((unsigned long)addr + sizeof(struct _chunk_header));
	pgpi->_chunk_pgcount = (size - (pgpi->_chunk_addr - addr)) / PG_SIZE;

	pgpi->_pgn_pool = malloc(sizeof(struct _pg_node) * pgpi->_chunk_pgcount);
	if(!pgpi->_pgn_pool) goto error_ret;

	pgpi->_flh = malloc(sizeof(struct _free_list_head) * pgpi->_cfg.freelist_count);
	if(!pgpi->_flh) goto error_ret;

	for(long i = 0; i < pgpi->_chunk_pgcount; ++i)
	{
		lst_clr(&pgpi->_pgn_pool[i]._free_pgn_node);
		lst_push_back(&pgpi->_free_pgn_list, &pgpi->_pgn_pool[i]._free_pgn_node);
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
		if(pgpi->_pgn_pool)
			free(pgpi->_pgn_pool);

		free(pgpi);
	}
}


void* pgp_alloc(struct pgpool* pgp, long size)
{
	void* payload;
	long flh_idx, rslt, pg_count;
	struct _pg_node* pgn;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	pg_count = (size + PG_SIZE - 1) >> PG_SIZE_SHIFT;

	rslt = _take_free_node(pgpi, pg_count, &pgn);
	if(rslt < 0 || !pgn) goto error_ret;

	payload = pgn->_payload_addr;
	pgn->using = 1;

	return payload;
error_ret:
	return 0;
}

long pgp_free(struct pgpool* pgp, void* payload)
{
	long rslt;
	struct _pg_node* pgn;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	if(((unsigned long)payload & (PG_SIZE - 1)) != 0)
		goto error_ret;

	pgn = _pgn_from_payload(pgpi, payload);
	if(!pgn || !pgn->using)
		goto error_ret;

	rslt = _return_free_node(pgpi, pgn);
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}


