#include <stdlib.h>
#include "mmpool.h"

#define MIN_BLOCK_SIZE (24)
#define FREE_LIST_COUNT	(32)
#define CHUNK_LABEL (0xabab1212dfdf6969)

#pragma pack(1)
// 8 bytes payload head
struct _block_head
{
	unsigned _flag : 4;
	unsigned _reserve : 12;
	unsigned long _block_size : 48;
};
#pragma pack()

// 8 bytes payload tail 
struct _block_tail
{
	unsigned long _prot_magic_num;
};

struct _free_list_node
{
	struct _block_head* _block;
	struct _free_list_node * _next_free_node;
};

struct _free_list_head
{
	struct _free_list_node* _head_node;
	long _node_count;
};

struct _mmpool_impl
{
	struct mmpool _pool;
	long _fln_count;
	long _next_aval_fln_idx;
	long _max_used_fln_idx;
	struct _free_list_node* _fln_pool;
	struct _free_list_head* _flh[FREE_LIST_COUNT];
};

struct _chunk_head
{
	unsigned long _chunck_label;
	unsigned long _reserved;
};

inline long _payload_size(long block_size)
{
	return block_size - sizeof(struct _block_head) - sizeof(struct _block_tail);
}

inline void* _payload_addr(void* block_addr)
{
	return block_addr + sizeof(struct _block_head);
}

inline long _block_size(long payload_size)
{
	return payload_size + sizeof(struct _block_head) + sizeof(struct _block_tail);
}

inline void* _block_addr(void* payload_addr)
{
	return payload_addr + sizeof(struct _block_head);
}

inline struct _block_head* _next_block(struct _block_head* _block)
{
	return (struct _block_head*)((void*)_block + _block->_block_size);
}

long _mmp_init(struct _mmpool_impl* mmpi)
{
	struct _chunk_head* hd = (struct _chunk_head*)(mmpi->_pool.mm_addr);
	if(!hd) goto error_ret;

	hd->_chunck_label = CHUNK_LABEL;
	hd->_reserved = 0;

	return 0;
error_ret:
	return -1;
}

long _mmp_load(struct _mmpool_impl* mmpi)
{

	return 0;
error_ret:
	return -1;
}


struct mmpool* mmp_new(void* addr, long size)
{
	struct _chunk_head* ch = 0;
	struct _mmpool_impl* mmpi = 0;
	long result = 0;

	if(!addr) goto error_ret;
	if(size < MIN_BLOCK_SIZE) goto error_ret;

	mmpi = malloc(sizeof(struct _mmpool_impl));
	if(!mmpi) goto error_ret;

	mmpi->_fln_count = size / MIN_BLOCK_SIZE;
	mmpi->_fln_pool = malloc(mmpi->_fln_count * sizeof(struct _free_list_node));
	if(!mmpi->_fln_pool) goto error_ret;

	mmpi->_next_aval_fln_idx = 0;
	mmpi->_max_used_fln_idx = 0;

	for(long i = 0; i < mmpi->_fln_count; ++i)
	{
		mmpi->_fln_pool[i]._next_free_node = 0;
		mmpi->_fln_pool[i]._block = 0;
	}

	for(long i = 0; i < FREE_LIST_COUNT; ++i)
	{
		mmpi->_flh[i] = 0;
	}

	mmpi->_pool.mm_addr = addr;
	mmpi->_pool.mm_size = size;

	ch = (struct _chunk_head*)addr;
	if(ch->_chunck_label == CHUNK_LABEL)
		result = _mmp_load(mmpi);
	else
		result = _mmp_init(mmpi);

	if(result < 0) goto error_ret;

	return &mmpi->_pool;

error_ret:

	if(mmpi)
	{
		if(mmpi->_fln_pool)
			free(mmpi->_fln_pool);

		free(mmpi);
	}

	return 0;
}

void mmp_del(struct mmpool* mmp)
{

}

void* mmp_alloc(long size)
{

}

void mmp_free(void* p)
{

}
