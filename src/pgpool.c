#include "pgpool.h"
#include "dlist.h"
#include <stdlib.h>

#define PG_SIZE (4096)
#define PGP_CHUNK_LABEL (0xbaba2121fdfd9696)

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
	long _idx;
};

struct _free_list_head
{
	struct dlist _free_list;
	long _op_count;
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
};


static inline unsigned long _align_pg(unsigned long addr)
{
	return (addr + (PG_SIZE - 1)) & (~(PG_SIZE - 1));
}

struct pgpool* pgp_new(void* addr, long size, struct pgpool_config* cfg)
{
	struct _chunk_header* hd;
	struct _pgpool_impl* pgpi;
	if(!addr || size <= PG_SIZE) goto error_ret;

	pgpi = (struct _pgpool_impl*)malloc(sizeof(struct _pgpool_impl));
	if(!pgpi) goto error_ret;

	pgpi->_the_pool.addr = addr;
	pgpi->_the_pool.size = size;

	pgpi->_chunk_addr = (void*)_align_pg((unsigned long)addr + sizeof(struct _chunk_header));
	pgpi->_chunk_pgcount = (size - (pgpi->_chunk_addr - addr)) / PG_SIZE;

	hd = pgpi->_chunk_addr - sizeof(struct _chunk_header);
	hd->_chunck_label = PGP_CHUNK_LABEL;
	hd->_reserved = 0;

	pgpi->_cfg.max_pg = cfg->max_pg;

	return &pgpi->_the_pool;
error_ret:
	return 0;
}

struct pgpool* pgp_load(void* addr, long size)
{

error_ret:
	return 0;
}


void pgp_del(struct pgpool* pgp)
{

}


void* pgp_alloc(long size)
{

error_ret:
	return 0;
}

long pgp_free(void* p)
{

}
