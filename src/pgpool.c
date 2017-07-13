#include "pgpool.h"
#include "dlist.h"
#include "misc.h"
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

struct _hash_list_node
{
	union
	{
		struct dlnode _lst_node;
		struct dlnode _free_hln_node;
	};
	unsigned long _pg_addr;
	long _pg_count;
};

struct _hash_list_head
{
	struct dlist _hash_list;
	long _node_count;
};

struct _pgpool_impl
{
	struct pgpool _the_pool;
	struct pgpool_config _cfg;

	void* _chunk_addr;
	long _chunk_pgcount;

	struct dlist _free_fln_list;
	struct _free_list_node* _fln_pool;
	struct _free_list_head* _flh;

	struct dlist _free_hln_list;
	struct _hash_list_node* _hln_pool;
	struct _hash_list_head _hlh[HASH_COUNT];
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

static inline struct _hash_list_node* _conv_hln(struct dlnode* dln)
{
	return (struct _hash_list_node*)((void*)dln - (unsigned long)(&((struct _hash_list_node*)(0))->_lst_node));
}

static inline struct _hash_list_node* _conv_free_hln(struct dlnode* dln)
{
	return (struct _hash_list_node*)((void*)dln - (unsigned long)(&((struct _hash_list_node*)(0))->_free_hln_node));
}

static inline struct _free_list_node* _fetch_free_fln(struct _pgpool_impl* pgpi)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_free_fln_list);
	struct _free_list_node* fln = _conv_free_fln(dln);

	return fln;
}

static inline struct _hash_list_node* _fetch_free_hln(struct _pgpool_impl* pgpi)
{
	struct dlnode* dln = lst_pop_front(&pgpi->_free_hln_list);
	struct _hash_list_node* hln = _conv_free_hln(dln);

	return hln;
}

static struct _free_list_node* _take_free_node(struct _pgpool_impl* pgpi, long flh_idx)
{

error_ret:
	return 0;
}

static void _putin_hash_list(struct _pgpool_impl* pgpi, void* pg_addr, long pg_count)
{
	long hash_idx = _hash_key((unsigned long)pg_addr);

}

static struct _hash_node* _find_in_hash_list(struct _pgpool_impl* pgpi, void* pg_addr)
{
	long hash_idx = _hash_key((unsigned long)pg_addr);

error_ret:
	return 0;
}

static long _pgp_init_chunk(struct _pgpool_impl* pgpi)
{
	struct _chunk_header* hd;
	long remain_count;
	long flh_idx;
	void* pg;

	hd = pgpi->_chunk_addr - sizeof(struct _chunk_header);
	remain_count = pgpi->_chunk_pgcount - pgpi->_cfg.max_pg_count;
	pg = pgpi->_chunk_addr;

	flh_idx = log_2(pgpi->_cfg.max_pg_count);

	while(remain_count > 0)
	{
		struct dlnode* dln = lst_pop_front(&pgpi->_free_fln_list);
		struct _free_list_node* fln = _conv_fln(dln);

		fln->_payload_addr = pg;
		if(remain_count >= pgpi->_cfg.max_pg_count)
			fln->_pg_count = pgpi->_cfg.max_pg_count;
		else
			fln->_pg_count = remain_count;

		pg += pgpi->_cfg.max_pg_count;
		remain_count -= pgpi->_cfg.max_pg_count;
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
	lst_new(&pgpi->_free_hln_list);

	pgpi->_the_pool.addr = addr;
	pgpi->_the_pool.size = size;
	pgpi->_cfg.max_pg_count = cfg->max_pg_count;

	pgpi->_chunk_addr = (void*)_align_pg((unsigned long)addr + sizeof(struct _chunk_header));
	pgpi->_chunk_pgcount = (size - (pgpi->_chunk_addr - addr)) / PG_SIZE;

	pgpi->_fln_pool = malloc(sizeof(struct _free_list_node) * pgpi->_chunk_pgcount);
	if(!pgpi->_fln_pool) goto error_ret;

	pgpi->_hln_pool = malloc(sizeof(struct _free_list_node) * pgpi->_chunk_pgcount);
	if(!pgpi->_hln_pool) goto error_ret;

	for(long i = 0; i < pgpi->_chunk_pgcount; ++i)
	{
		lst_clr(&pgpi->_fln_pool[i]._lst_node);
		lst_push_back(&pgpi->_free_fln_list, &pgpi->_fln_pool[i]._free_fln_node);

		lst_clr(&pgpi->_hln_pool[i]._lst_node);
		lst_push_back(&pgpi->_free_hln_list, &pgpi->_hln_pool[i]._free_hln_node);
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

}


void* pgp_alloc(struct pgpool* pgp, long size)
{
	long flh_idx;
	struct _free_list_node* fln;
	struct _pgpool_impl* pgpi = _conv_impl(pgp);

	size = (size + PG_SIZE - 1) >> PG_SIZE_SHIFT;
	flh_idx = log_2(size);

	fln = _take_free_node(pgpi, flh_idx);
	if(!fln) goto error_ret;

	_putin_hash_list(pgpi, fln->_payload_addr, fln->_pg_count);

	return fln->_payload_addr;
error_ret:
	return 0;
}

long pgp_free(struct pgpool* up, void* p)
{

}
