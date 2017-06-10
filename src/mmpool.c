#include <stdlib.h>
#include <string.h>
#include "mmpool.h"
#include "misc.h"

#define FREE_LIST_COUNT	(17)	// payload size: MIN_PAYLOAD_SIZE * (1 << list_idx) <list_idx: 0 ~ FREE_LIST_COUNT - 1>
#define CHUNK_LABEL (0xabab1212dfdf6969)
#define TAIL_LABEL (0x1a2b3c4d)


#pragma pack(1)
// 8 bytes payload head
struct _block_head
{
	unsigned _flag : 4;
	unsigned _fln_idx : 28;
	unsigned int _block_size;
};

// 8 bytes payload tail 
struct _block_tail
{
	unsigned int _tail_label;
	unsigned int _block_size;
};

#pragma pack()

#define BLOCK_BIT_USED			1
#define BLOCK_BIT_BUDDY_PREV	2
#define BLOCK_BIT_BUDDY_POST	4
#define BLOCK_BIT_UNM			8

#define HEAD_TAIL_SIZE (sizeof(struct _block_head) + sizeof(struct _block_tail))

#define MIN_BLOCK_SIZE (32)
#define MAX_BLOCK_SIZE (MIN_BLOCK_SIZE * (1 << (FREE_LIST_COUNT - 1)))

#define MIN_PAYLOAD_SIZE (MIN_BLOCK_SIZE - HEAD_TAIL_SIZE)
#define MAX_PAYLOAD_SIZE (MAX_BLOCK_SIZE - HEAD_TAIL_SIZE)

struct _free_list_node
{
	struct _block_head* _block;
	struct _free_list_node * _next_free_node;
	long _idx;
};

struct _free_list_head
{
	struct _free_list_node* _head_node;
	long _node_count;
	long _alloc_times;
};

struct _mmpool_impl
{
	struct mmpool _pool;
	long _fln_count;
	long _next_aval_fln_idx;
	long _max_used_fln_idx;
	struct _free_list_node* _fln_pool;
	struct _free_list_head _flh[FREE_LIST_COUNT];
};

struct _chunk_head
{
	unsigned long _chunck_label;
	unsigned long _reserved;
};

static long _return_free_node(struct _mmpool_impl* mmpi, struct _block_head* hd);
static long _take_free_node(struct _mmpool_impl* mmpi, long flh_idx, struct _block_head** bh);
static long  _mmp_init_block(struct _mmpool_impl* mmpi, void* blk, long head_idx);
static long _mmp_load_chunk(struct _mmpool_impl* mmpi);


inline long _payload_size(long block_size)
{
	return block_size - HEAD_TAIL_SIZE;
}

inline void* _payload_addr(void* block_addr)
{
	return block_addr + sizeof(struct _block_head);
}

inline long _block_size(long payload_size)
{
	return payload_size + HEAD_TAIL_SIZE;
}

inline long _block_size_idx(long free_list_idx)
{
	return MIN_BLOCK_SIZE * (1 << free_list_idx);
}

inline void* _block_head(void* payload_addr)
{
	return (struct _block_head*)(payload_addr + sizeof(struct _block_head));
}

inline struct _block_tail* _get_tail(struct _block_head* bh)
{
	return (struct _block_tail*)(bh + bh->_block_size - sizeof(struct _block_tail));
}

inline struct _block_head* _prev_block(struct _block_head* bh)
{
	struct _block_tail* tl = (struct _block_tail*)((void*)bh - sizeof(struct _block_tail));
	return (struct _block_head*)((void*)bh - tl->_block_size);
}

inline struct _block_head* _next_block(struct _block_head* bh)
{
	return (struct _block_head*)((void*)bh + bh->_block_size);
}

inline long _normalize_payload_size(long payload_size)
{
	long blk_size = _block_size(payload_size);
	blk_size = align_to_2power_top(blk_size);
	return _payload_size(blk_size);
}

inline long _normalize_block_size(long payload_size)
{
	long blk_size = _block_size(payload_size);
	blk_size = align_to_2power_top(blk_size);

	return blk_size;
}

inline long _flh_idx(long payload_size)
{
	long blk_size = _normalize_block_size(payload_size);
	if(blk_size < MIN_BLOCK_SIZE || blk_size > MAX_BLOCK_SIZE) goto error_ret;

	return log_2(blk_size / MIN_BLOCK_SIZE);
error_ret:
	return -1;
}

inline void _make_block_tail(struct _block_tail* tail, unsigned int block_size)
{
	tail->_tail_label = TAIL_LABEL;
	tail->_block_size = block_size;
}

inline struct _block_head* _make_block(void* addr, unsigned int block_size)
{
	struct _block_head* head = 0;
	struct _block_tail* tail = 0;

	if(!is_2power(block_size)) goto error_ret;

	head = (struct _block_head*)addr;
	head->_block_size = block_size;
	head->_fln_idx = log_2(block_size) / MIN_BLOCK_SIZE;
	head->_flag = 0;

	tail = _get_tail(head);
	_make_block_tail(tail, block_size);

	return head;
error_ret:
	return 0;
}

inline struct _block_head* _make_unmblock(void* addr, unsigned int block_size)
{
	struct _block_head* head = 0;
	struct _block_tail* tail = 0;

	head = (struct _block_head*)addr;
	head->_block_size = block_size;
	head->_fln_idx = 0;
	head->_flag = BLOCK_BIT_UNM;

	tail = _get_tail(head);
	_make_block_tail(tail, block_size);

	return head;
error_ret:
	return 0;
}

static long _return_free_node(struct _mmpool_impl* mmpi, struct _block_head* hd)
{
	long fln_idx = 0;
	long flh_idx = 0;

	if(hd->_flag != 0) goto error_ret;
	if(hd->_block_size > MAX_BLOCK_SIZE) goto error_ret;
	if(mmpi->_max_used_fln_idx >= mmpi->_fln_count) goto error_ret;

	flh_idx = log_2(hd->_block_size / MIN_BLOCK_SIZE);

	fln_idx = mmpi->_next_aval_fln_idx;

	if(mmpi->_next_aval_fln_idx < 0)
		fln_idx = ++mmpi->_max_used_fln_idx;
	else
		mmpi->_next_aval_fln_idx = -1;

	mmpi->_fln_pool[fln_idx]._block = hd;
	mmpi->_fln_pool[fln_idx]._next_free_node = mmpi->_flh[flh_idx]._head_node;

	mmpi->_flh[flh_idx]._head_node = &mmpi->_fln_pool[fln_idx];
	++mmpi->_flh[flh_idx]._node_count;

	return 0;
error_ret:
	return -1;
}

static long _take_free_node(struct _mmpool_impl* mmpi, long flh_idx, struct _block_head** rbh)
{
	struct _free_list_node* fln = 0;
	struct _block_head* bh = 0;

	*rbh = 0;

	while(1)
	{
		if(flh_idx >= FREE_LIST_COUNT) goto error_ret;

		fln = mmpi->_flh[flh_idx]._head_node;
		if(fln != 0)
			break;

		++flh_idx;
	}

	mmpi->_flh[flh_idx]._head_node = fln->_next_free_node;
	--mmpi->_flh[flh_idx]._node_count;

	mmpi->_next_aval_fln_idx = fln->_idx;
	*rbh = fln->_block;
	(*rbh)->_fln_idx = fln->_idx;

	return 0;
error_ret:
	return -1;
}

static long _try_spare_block(struct _mmpool_impl* mmpi, struct _block_head* bh, unsigned int payload_size)
{
	struct _block_head* h = 0;
	struct _block_tail* t = 0;
	unsigned int half_size = 0;
	void* end = 0, *sp = 0, *mid = 0;

	if(!is_2power(bh->_block_size)) goto error_ret;

	payload_size = align8(payload_size);

	end = (void*)bh + bh->_block_size;
	sp = (void*)bh + HEAD_TAIL_SIZE + payload_size;

	// binary split the block into small piece of 2-power size if possible.

	half_size = (bh->_block_size >> 1);
	mid = (void*)bh + half_size;

	while(mid > sp && half_size > MIN_BLOCK_SIZE)
	{
		h = _make_block(mid, half_size);
		_return_free_node(mmpi, h);
		
		half_size >>= 1;
		mid = (void*)bh + half_size;
	}

	end = (void*)bh + bh->_block_size;

	// the last step.
	//
	unsigned long offset = align_to_2power_floor((unsigned long)end - (unsigned long)sp);
	if(offset < MIN_BLOCK_SIZE) goto succ_ret;

	// if can spare a small block of 2-power size, just do it.
	//
	sp = end - offset;

	h = _make_block(sp, offset);
	h->_flag |= BLOCK_BIT_BUDDY_POST;
	_return_free_node(mmpi, h);

	// remain block is the block for the payload, with buddy flag, so bh->_blk_size remains the original size;
	// bh is for the block head, and t is for the block tail.
	//
	_make_unmblock(bh, _block_size(payload_size));
	bh->_flag |= BLOCK_BIT_BUDDY_PREV;

succ_ret:
	return 0;
error_ret:
	return -1;
}

static long  _mmp_init_block(struct _mmpool_impl* mmpi, void* blk, long head_idx)
{
	long rslt = 0;
	struct _block_head* hd = (struct _block_head*)blk;
	struct _block_tail* tl = 0;

	hd->_flag = 0;
	hd->_fln_idx = 0;
	hd->_block_size = _block_size_idx(head_idx);

	tl = _get_tail(hd);

	_make_block_tail(tl, hd->_block_size);

	rslt = _return_free_node(mmpi, hd);
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}

static long _mmp_init_chunk(struct _mmpool_impl* mmpi)
{
	void* chk1 = 0;
	void* chk2 = 0;
	long blk_size = 0;
	long rslt = 0;

	struct _chunk_head* hd = (struct _chunk_head*)(mmpi->_pool.mm_addr);
	if(!hd) goto error_ret;

	hd->_chunck_label = CHUNK_LABEL;
	hd->_reserved = 0;

	chk1 = mmpi->_pool.mm_addr + sizeof(struct _chunk_head);

	for(long idx = FREE_LIST_COUNT - 1; idx > 0; idx >>= 1)
	{
		blk_size = _block_size_idx(idx);

		chk2 = chk1 + blk_size;

		while(chk2 <= mmpi->_pool.mm_addr + mmpi->_pool.mm_size)
		{
			rslt = _mmp_init_block(mmpi, chk1, idx);
			if(rslt < 0) goto error_ret;

			chk1 = chk2;
			chk2 = chk1 + blk_size;
		}
	}

	return 0;
error_ret:
	return -1;
}

static long _mmp_load_chunk(struct _mmpool_impl* mmpi)
{
	void* chk = 0;
	long blk_size = 0;
	long payload_size = 0;
	long rslt = 0;
	struct _block_head* bhd = 0;

	struct _chunk_head* chd = (struct _chunk_head*)(mmpi->_pool.mm_addr);
	if(!chd) goto error_ret;
	if(chd->_chunck_label != CHUNK_LABEL) goto error_ret;

	chk = mmpi->_pool.mm_addr + sizeof(struct _chunk_head);
	while(chk <= mmpi->_pool.mm_addr + mmpi->_pool.mm_size)
	{
		bhd = (struct _block_head*)chk;
		if(bhd->_flag & BLOCK_BIT_USED) continue;

		payload_size = _payload_size(bhd->_block_size);

		rslt = _return_free_node(mmpi, bhd);
		if(rslt < 0) goto error_ret;
	}

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
	if(size > 0xFFFFFFFF) goto error_ret;

	mmpi = malloc(sizeof(struct _mmpool_impl));
	if(!mmpi) goto error_ret;

	mmpi->_fln_count = size / MIN_BLOCK_SIZE;
	mmpi->_fln_pool = malloc(mmpi->_fln_count * sizeof(struct _free_list_node));
	if(!mmpi->_fln_pool) goto error_ret;

	mmpi->_next_aval_fln_idx = -1;
	mmpi->_max_used_fln_idx = -1;

	for(long i = 0; i < mmpi->_fln_count; ++i)
	{
		mmpi->_fln_pool[i]._next_free_node = 0;
		mmpi->_fln_pool[i]._block = 0;
		mmpi->_fln_pool[i]._idx = i;
	}

	for(long i = 0; i < FREE_LIST_COUNT; ++i)
	{
		mmpi->_flh[i]._head_node = 0;
		mmpi->_flh[i]._node_count = 0;
		mmpi->_flh[i]._alloc_times = 0;
	}

	mmpi->_pool.mm_addr = addr;
	mmpi->_pool.mm_size = size;

	ch = (struct _chunk_head*)addr;
	if(ch->_chunck_label == CHUNK_LABEL)
		result = _mmp_load_chunk(mmpi);
	else
		result = _mmp_init_chunk(mmpi);

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
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;

	if(mmpi)
	{
		if(mmpi->_fln_pool)
			free(mmpi->_fln_pool);

		free(mmpi);
	}
}

void* mmp_alloc(struct mmpool* mmp, long payload_size)
{
	long rslt = 0;
	long flh_idx = 0;
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;
	struct _block_head* bh = 0;

	if(payload_size > 0xFFFFFFFF) goto error_ret;
	if(payload_size > MAX_PAYLOAD_SIZE) goto error_ret;

	flh_idx = _flh_idx(payload_size);
	rslt = _take_free_node(mmpi, flh_idx, &bh);
	if(rslt < 0) goto error_ret;

	rslt = _try_spare_block(mmpi, bh, payload_size);
	if(rslt < 0) goto error_ret;

	bh->_flag |= BLOCK_BIT_USED;

	return _payload_addr(bh);
error_ret:
	return 0;
}

int mmp_free(struct mmpool* mmp, void* p)
{
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;
	struct _block_head* bh = (struct _block_head*)p;
	
	if((bh->_flag & BLOCK_BIT_USED) == 0) goto error_ret;
	bh->_flag ^= BLOCK_BIT_USED;

	if(bh->_flag & BLOCK_BIT_UNM)
	{
		if((bh->_flag & BLOCK_BIT_BUDDY_PREV) == 0) goto error_ret;

		// wait for buddy.
		goto succ_ret;
	}



succ_ret:
	return 0;
error_ret:
	return -1;
}


