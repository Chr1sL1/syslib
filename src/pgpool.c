#include "pgpool.h"
#include "dlist.h"
#include "misc.h"
#include "rbtree.h"
#include <stdio.h>

#define PG_SIZE			(4096)
#define PG_SIZE_SHIFT	(12)

#define PGP_CHUNK_LABEL (0xbaba2121fdfd9696UL)
#define HASH_COUNT		(256)

#pragma pack(1)

struct _chunk_header
{
	unsigned long _chunck_label;
	unsigned long _addr_begin;
	unsigned long _addr_end;
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
}__attribute__((aligned(8)));

struct _free_list_head
{
	struct dlist _free_list;
	long _op_count;
}__attribute__((aligned(8)));

struct _pgp_cfg
{
	unsigned long maxpg_count;
	unsigned long freelist_count;
}__attribute__((aligned(8)));

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
}__attribute__((aligned(8)));

static inline unsigned long _align_pg(unsigned long addr)
{
	return (addr + (PG_SIZE - 1)) & (~(PG_SIZE - 1));
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

static inline void _set_payload(struct _pg_node* node, void* payload)
{
	node->_payload_addr = (void*)((unsigned long)payload | node->using);
}

static inline void* _get_payload(struct _pg_node* node)
{
	return (void*)(((unsigned long)(node->_payload_addr)) & (~3));
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
	struct _pg_node* pgn;
	struct dlnode* fln = lst_pop_front(&pgpi->_flh[flh_idx]._free_list);
	pgn = _conv_fln(fln);

//	if(lst_check(&pgpi->_flh[flh_idx]._free_list) < 0)
//		goto error_ret;

	return pgn;
error_ret:
	return 0;
}

static inline long _link_fln(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long rslt;
	long flh_idx = log_2(pgn->_pg_count);

	lst_clr(&pgn->_fln_node);

	rslt = lst_push_front(&pgpi->_flh[flh_idx]._free_list, &pgn->_fln_node);

//	if(lst_check(&pgpi->_flh[flh_idx]._free_list) < 0)
//		goto error_ret;

	return rslt;
error_ret:
	return -1;
}

static inline long _unlink_fln(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long rslt;
	long flh_idx = log_2(pgn->_pg_count);
	rslt = lst_remove(&pgpi->_flh[flh_idx]._free_list, &pgn->_fln_node);

//	if(lst_check(&pgpi->_flh[flh_idx]._free_list) < 0)
//		goto error_ret;

	return rslt;
error_ret:
	return -1;
}

static inline long _link_rbn(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	rb_fillnew(&pgn->_rb_node);
	pgn->_rb_node.key = _get_payload(pgn);

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

	rbn = rb_search(&pgpi->_pgn_tree, payload, &hot);
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
			if(candi_pgn->_pg_count >= pg_count && !candi_pgn->using)
			{
				_unlink_fln(pgpi, candi_pgn);
				goto candi_found;
			}
			candi_pgn = 0;

			dln = dln->next;
		}
	}

candi_found:	

	if(!candi_pgn) goto error_ret;

	*pgn = candi_pgn;

	if(candi_pgn->_pg_count > pg_count)
	{
		struct _pg_node* new_pgn = _fetch_free_pgn(pgpi);
		_set_payload(new_pgn, _get_payload(candi_pgn) + pg_count * PG_SIZE);
		new_pgn->_pg_count = candi_pgn->_pg_count - pg_count;
		candi_pgn->_pg_count = pg_count;

		_link_rbn(pgpi, new_pgn);
		_link_fln(pgpi, new_pgn);
	}

succ_ret:
	return 0;
error_ret:
	return -1;
}

static inline long _merge_free_node(struct _pgpool_impl* pgpi, struct _pg_node* prev, struct _pg_node* next)
{
	if(_get_payload(next) != _get_payload(prev) + prev->_pg_count * PG_SIZE)
		goto error_ret;

	_unlink_fln(pgpi, next);
	_unlink_rbn(pgpi, next);

	prev->_pg_count = prev->_pg_count + next->_pg_count;

	_return_free_pgn(pgpi, next);

	return 0;
error_ret:
	return -1;
}

static long _return_free_node(struct _pgpool_impl* pgpi, struct _pg_node* pgn)
{
	long found = 0;
	long rslt = 0;
	struct rbnode* succ;
	struct _pg_node* succ_pgn;

	succ = rb_succ(&pgn->_rb_node);
	while(succ && rslt >= 0)
	{
		succ_pgn = _conv_rbn(succ);
		if(succ_pgn->using || pgn->_pg_count + succ_pgn->_pg_count > pgpi->_cfg.maxpg_count)
			break;

		rslt = _merge_free_node(pgpi, pgn, succ_pgn);
		succ = rb_succ(&pgn->_rb_node);
	}

succ_ret:
	_link_fln(pgpi, pgn);
	return 0;
error_ret:
	return -1;
}

static struct _pgpool_impl* _pgp_init_chunk(void* addr, unsigned long size, unsigned long maxpg_count)
{
	struct _pgpool_impl* pgpi;
	struct _chunk_header* hd;
	long remain_count, flh_idx, rslt;
	void* pg;
	void* cur_offset;
	unsigned long chunk_pg_count;

	hd = (struct _chunk_header*)addr;
	cur_offset = move_ptr_align64(addr, sizeof(struct _chunk_header));

	if(hd->_chunck_label == PGP_CHUNK_LABEL)
		goto error_ret;

	hd->_chunck_label = PGP_CHUNK_LABEL;

	pgpi = (struct _pgpool_impl*)cur_offset;
	cur_offset = move_ptr_align64(cur_offset, sizeof(struct _pgpool_impl));

	pgpi->_the_pool.addr_begin = addr;
	pgpi->_the_pool.addr_end = (void*)_align_pg((unsigned long)addr + size) - PG_SIZE;

	hd->_addr_begin = (unsigned long)pgpi->_the_pool.addr_begin;
	hd->_addr_end = (unsigned long)pgpi->_the_pool.addr_end;

	lst_new(&pgpi->_free_pgn_list);
	rb_init(&pgpi->_pgn_tree, 0);

	pgpi->_cfg.maxpg_count = maxpg_count;
	pgpi->_cfg.freelist_count = log_2(maxpg_count) + 1;

	pgpi->_flh = (struct _free_list_head*)cur_offset;
	cur_offset += sizeof(struct _free_list_head) * pgpi->_cfg.freelist_count;

	for(long i = 0; i < pgpi->_cfg.freelist_count; ++i)
	{
		lst_new(&pgpi->_flh[i]._free_list);
	}

	pgpi->_chunk_pgcount = (pgpi->_the_pool.addr_end - cur_offset) / (PG_SIZE + sizeof(struct _pg_node));

	pgpi->_pgn_pool = (struct _pg_node*)cur_offset;
	cur_offset = move_ptr_align64(cur_offset, sizeof(struct _pg_node) * pgpi->_chunk_pgcount);
	cur_offset = (void*)_align_pg((unsigned long)cur_offset);

	pgpi->_chunk_addr = cur_offset;

	for(long i = 0; i < pgpi->_chunk_pgcount; ++i)
	{
		lst_clr(&pgpi->_pgn_pool[i]._free_pgn_node);
		lst_push_back(&pgpi->_free_pgn_list, &pgpi->_pgn_pool[i]._free_pgn_node);
	}

	chunk_pg_count = (pgpi->_the_pool.addr_end - cur_offset) / PG_SIZE;

	remain_count = pgpi->_chunk_pgcount - pgpi->_cfg.maxpg_count;
	pg = pgpi->_chunk_addr;

	while(remain_count > 0)
	{
		struct _pg_node* pgn = _fetch_free_pgn(pgpi);

		_set_payload(pgn, pg);
		if(remain_count >= pgpi->_cfg.maxpg_count)
			pgn->_pg_count = pgpi->_cfg.maxpg_count;
		else
			pgn->_pg_count = remain_count;

		_link_rbn(pgpi, pgn);
		_link_fln(pgpi, pgn);

		if(rslt < 0) goto error_ret;

		pg += pgn->_pg_count * PG_SIZE;
		remain_count -= pgn->_pg_count;
	}

	return pgpi;
error_ret:
	return 0;
}

static struct _pgpool_impl* _pgp_load_chunk(void* addr)
{
	void* cur_offset;
	struct _chunk_header* hd;
	struct _pgpool_impl* pgpi;

	hd = (struct _chunk_header*)addr;
	cur_offset = move_ptr_align64(addr, sizeof(struct _chunk_header));

	if(hd->_chunck_label != PGP_CHUNK_LABEL)
		goto error_ret;

	if(hd->_addr_begin != (unsigned long)addr)
		goto error_ret;

	pgpi = (struct _pgpool_impl*)cur_offset;
	if(pgpi->_the_pool.addr_begin != addr)
		goto error_ret;

	return pgpi;
error_ret:
	return 0;
}

struct pgpool* pgp_create(void* addr, unsigned long size, unsigned long maxpg_count)
{
	long rslt = 0;
	struct _pgpool_impl* pgpi;

	if(!addr || ((unsigned long)addr & 0x7) != 0 || size <= PG_SIZE) goto error_ret;

	pgpi = _pgp_init_chunk(addr, size, maxpg_count);
	if(!pgpi) goto error_ret;

	return &pgpi->_the_pool;
error_ret:
	if(pgpi)
		pgp_destroy(&pgpi->_the_pool);

	return 0;
}

struct pgpool* pgp_load(void* addr)
{
	struct _pgpool_impl* pgpi;

	if(!addr || ((unsigned long)addr & 0x7) != 0) goto error_ret;

	pgpi = _pgp_load_chunk(addr);
	if(!pgpi) goto error_ret;

	return &pgpi->_the_pool;
error_ret:
	if(pgpi)
		pgp_destroy(&pgpi->_the_pool);

	return 0;
}

void pgp_destroy(struct pgpool* pgp)
{
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	if(pgpi)
	{
		struct _chunk_header* hd = (struct _chunk_header*)(pgpi->_the_pool.addr_begin);
		if(hd->_chunck_label == PGP_CHUNK_LABEL)
		{
			hd->_chunck_label = 0;
			hd->_addr_begin = 0;
			hd->_addr_end = 0;
		}
	}
}


void* pgp_alloc(struct pgpool* pgp, unsigned long size)
{
	void* payload;
	long flh_idx, rslt, pg_count;
	struct _pg_node* pgn;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	pg_count = (size + PG_SIZE - 1) >> PG_SIZE_SHIFT;

	rslt = _take_free_node(pgpi, pg_count, &pgn);
	if(rslt < 0 || !pgn) goto error_ret;

	payload = _get_payload(pgn);
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

	pgn->using = 0;
	rslt = _return_free_node(pgpi, pgn);
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}

long pgp_check(struct pgpool* pgp)
{
	long rslt = 0;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	for(long i = 0; i < pgpi->_cfg.freelist_count; ++i)
	{
		rslt = lst_check(&pgpi->_flh[i]._free_list);
		if(rslt < 0)
		{
			printf("!!! loop in freelist [%ld] !!!\n", i);
			return -1;
		}
	}

	return rslt;
}

