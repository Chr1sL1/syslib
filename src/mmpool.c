#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mmpool.h"
#include "misc.h"
#include "dlist.h"

// absolutely not thread-safe.

//#define FREE_LIST_COUNT	(17)	// payload size: MIN_PAYLOAD_SIZE * (1 << list_idx) <list_idx: 0 ~ FREE_LIST_COUNT - 1>
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
#define BLOCK_BIT_UNM			2
#define BLOCK_BIT_UNM_SUC		4	

#define PAGE_SIZE				0x1000

#define HEAD_TAIL_SIZE (sizeof(struct _block_head) + sizeof(struct _block_tail))

//#define MIN_BLOCK_SIZE (32)
//#define MAX_BLOCK_SIZE (MIN_BLOCK_SIZE * (1 << (FREE_LIST_COUNT - 1)))
//
//#define MIN_PAYLOAD_SIZE (MIN_BLOCK_SIZE - HEAD_TAIL_SIZE)
//#define MAX_PAYLOAD_SIZE (MAX_BLOCK_SIZE - HEAD_TAIL_SIZE)

struct _free_list_node
{
	union
	{
		struct dlnode _list_node;
		struct dlnode _free_fln_node;
	};

	struct _block_head* _block;
	long _idx;
};

struct _free_list_head
{
	struct dlist _free_list;
	long _op_count;
};

struct _mmpool_cfg
{
	long _min_block_idx;
	long _max_block_idx;

	long _free_list_count;
	long _min_payload_size;
	long _max_payload_size;
};

struct _mmpool_impl
{
	struct mmpool _pool;
	struct _mmpool_cfg _cfg;

	void* _chunk_addr;
	long _chunk_size;

	unsigned long _fln_count;
	struct dlist _free_fln_list;
	struct _free_list_node* _fln_pool;
	struct _free_list_head* _flh;
};

struct _chunk_head
{
	unsigned long _chunck_label;
	unsigned long _reserved;
};

static long _return_free_node_to_head(struct _mmpool_impl* mmpi, struct _block_head* hd);
static long _return_free_node_to_tail(struct _mmpool_impl* mmpi, struct _block_head* hd);
static long _take_free_node(struct _mmpool_impl* mmpi, long flh_idx, struct _block_head** bh);
static long  _mmp_init_block(struct _mmpool_impl* mmpi, void* blk, long head_idx);
static long _mmp_load_chunk(struct _mmpool_impl* mmpi);

static inline long _block_size(long idx)
{
	return (1 << idx);
}

static inline long _payload_size(long block_size)
{
	return block_size - HEAD_TAIL_SIZE;
}

static inline void* _payload_addr(void* block_addr)
{
	return block_addr + sizeof(struct _block_head);
}

static inline long _block_size_by_payload(long payload_size)
{
	return payload_size + HEAD_TAIL_SIZE;
}

static inline long _block_size_by_idx(struct _mmpool_impl* mmpi, long free_list_idx)
{
	return _block_size(mmpi->_cfg._min_block_idx) * (1 << free_list_idx);
}

static inline struct _free_list_head* _get_flh(struct _mmpool_impl* mmpi, struct _block_head* bh)
{
	long idx = log_2(bh->_block_size) - mmpi->_cfg._min_block_idx;

	if(idx >= mmpi->_cfg._free_list_count) goto error_ret;

	return &mmpi->_flh[idx];
error_ret:
	return 0;
}

static inline struct _free_list_node* _fetch_free_fln(struct _mmpool_impl* mmpi)
{
	struct dlnode* dln = lst_pop_front(&mmpi->_free_fln_list);
	struct _free_list_node* fln = (struct _free_list_node*)((void*)dln - (void*)(&((struct _free_list_node*)0)->_free_fln_node));


	return fln;
error_ret:
	return 0;
}

static inline void _return_free_fln(struct _mmpool_impl* mmpi, struct _free_list_node* fln)
{
	lst_clr(&fln->_free_fln_node);
	lst_push_front(&mmpi->_free_fln_list, &fln->_free_fln_node);
}

static inline void* _block_head(void* payload_addr)
{
	return (struct _block_head*)(payload_addr + sizeof(struct _block_head));
}

static inline struct _block_tail* _get_tail(struct _block_head* bh)
{
	return (struct _block_tail*)((void*)bh + bh->_block_size - sizeof(struct _block_tail));
}

static inline struct _block_head* _prev_block(struct _block_head* bh)
{
	struct _block_tail* tl = (struct _block_tail*)((void*)bh - sizeof(struct _block_tail));
	return (struct _block_head*)((void*)bh - tl->_block_size);
}

static inline struct _block_head* _next_block(struct _block_head* bh)
{
	return (struct _block_head*)((void*)bh + bh->_block_size);
}

static inline long _normalize_payload_size(long payload_size)
{
	long blk_size = _block_size_by_payload(payload_size);
	blk_size = align_to_2power_top(blk_size);
	return _payload_size(blk_size);
}

static inline long _normalize_block_size(long payload_size)
{
	long blk_size = _block_size_by_payload(payload_size);
	blk_size = align_to_2power_top(blk_size);

	return blk_size;
}

static inline long _flh_idx(struct _mmpool_impl* mmpi, long payload_size)
{
	long blk_size = _normalize_block_size(payload_size);
	if(blk_size < _block_size(mmpi->_cfg._min_block_idx) || blk_size > _block_size(mmpi->_cfg._max_block_idx)) goto error_ret;

	return log_2(blk_size) - mmpi->_cfg._min_block_idx;
error_ret:
	return -1;
}

static inline void _make_block_tail(struct _block_tail* tail, unsigned int block_size)
{
	tail->_tail_label = TAIL_LABEL;
	tail->_block_size = block_size;
}

static inline struct _block_head* _make_block(void* addr, unsigned int block_size)
{
	struct _block_head* head = 0;
	struct _block_tail* tail = 0;

	if(!is_2power(block_size)) goto error_ret;

	head = (struct _block_head*)addr;
	head->_block_size = block_size;
	head->_flag = 0;

	tail = _get_tail(head);
	_make_block_tail(tail, block_size);

	return head;
error_ret:
	return 0;
}

static inline struct _block_head* _make_unmblock(void* addr, unsigned int block_size)
{
	struct _block_head* head = 0;
	struct _block_tail* tail = 0;

	head = (struct _block_head*)addr;
	head->_block_size = block_size;
	head->_flag = BLOCK_BIT_UNM;

	tail = _get_tail(head);
	_make_block_tail(tail, block_size);

	return head;
}

static long _return_free_node_to_head(struct _mmpool_impl* mmpi, struct _block_head* hd)
{
	long rslt;
	long flh_idx;
	struct _free_list_node* fln;

	if((hd->_flag & BLOCK_BIT_USED) != 0) goto error_ret;
	if(hd->_block_size > _block_size(mmpi->_cfg._max_block_idx)) goto error_ret;

	flh_idx = log_2(hd->_block_size) - mmpi->_cfg._min_block_idx;

	fln = _fetch_free_fln(mmpi);
	if(!fln) goto error_ret;

	fln->_block = hd;
	hd->_fln_idx = fln->_idx;
	lst_clr(&fln->_list_node);
	rslt = lst_push_front(&mmpi->_flh[flh_idx]._free_list, &fln->_list_node);

	if(rslt < 0) goto error_ret;

	++mmpi->_flh[flh_idx]._op_count;

	return 0;
error_ret:
	return -1;
}

static long _return_free_node_to_tail(struct _mmpool_impl* mmpi, struct _block_head* hd)
{
	long rslt;
	long flh_idx;
	struct _free_list_node* fln;

	if((hd->_flag & BLOCK_BIT_USED) != 0) goto error_ret;
	if(hd->_block_size > _block_size(mmpi->_cfg._max_block_idx)) goto error_ret;

	flh_idx = log_2(hd->_block_size) - mmpi->_cfg._min_block_idx;

	fln = _fetch_free_fln(mmpi);
	if(!fln) goto error_ret;

	fln->_block = hd;
	hd->_fln_idx = 0;
	rslt = lst_push_back(&mmpi->_flh[flh_idx]._free_list, &fln->_list_node);

	if(rslt < 0) goto error_ret;

	++mmpi->_flh[flh_idx]._op_count;

	return 0;
error_ret:
	return -1;
}

// similar to the page-fault process
static long _take_free_node(struct _mmpool_impl* mmpi, long flh_idx, struct _block_head** rbh)
{
	struct _free_list_node* fln = 0;
	long idx = flh_idx;

	*rbh = 0;

	while(idx < mmpi->_cfg._free_list_count && mmpi->_flh[idx]._free_list.size <= 0)
	{
		++idx;
	}

	if(idx < mmpi->_cfg._free_list_count)
		fln = (struct _free_list_node*)lst_pop_front(&mmpi->_flh[idx]._free_list);

	if(!fln) goto error_ret;

	if(idx > flh_idx)
	{
		struct _block_head* bh = fln->_block;
		struct _block_head* h;
		long new_size = bh->_block_size;

		for(long i = 0; i < idx - flh_idx; ++i)
		{
			new_size >>= 1;
			h = (void*)bh + new_size;
			h = _make_block(h, new_size);
			_return_free_node_to_head(mmpi, h);
		}

		bh = _make_block(bh, new_size);
		_return_free_node_to_head(mmpi, bh);

		goto succ_ret;
	}

	*rbh = fln->_block;
	(*rbh)->_fln_idx = 0;

	_return_free_fln(mmpi, fln);

succ_ret:
	return 0;
error_ret:
	return -1;
}

static long _try_spare_block(struct _mmpool_impl* mmpi, struct _block_head* bh, unsigned int payload_size)
{
	struct _block_head* h;
	long offset;
	void* end;

	if(bh->_flag != 0) goto error_ret;
	else if(!is_2power(bh->_block_size)) goto error_ret;

	payload_size = align8(payload_size);

	if(payload_size <= mmpi->_cfg._min_payload_size) goto succ_ret;

	end = (void*)bh + bh->_block_size;
	offset = (long)bh->_block_size - HEAD_TAIL_SIZE - payload_size;

	if(offset > 0)
		offset = align_to_2power_floor(offset);

	if(offset >= _block_size(mmpi->_cfg._min_block_idx))
	{
		h = _make_block(end - offset, offset);
		h->_flag |= BLOCK_BIT_UNM_SUC;
		_return_free_node_to_head(mmpi, h);

		bh = _make_unmblock(bh, bh->_block_size - offset);
	}

	// remain block is the block for the payload, with buddy flag, so bh->_blk_size remains the original size;
	// bh is for the block head, and t is for the block tail.
	//

succ_ret:
	return 0;
error_ret:
	return -1;
}

static inline void _unlink_block(struct _mmpool_impl* mmpi, struct _block_head* bh)
{
	struct _free_list_node* fln = &mmpi->_fln_pool[bh->_fln_idx];
	struct _free_list_head* flh = _get_flh(mmpi, bh);
	lst_remove(&flh->_free_list, &fln->_list_node);

	_return_free_fln(mmpi, fln);
}

static long _merge_free_blocks(struct _mmpool_impl* mmpi, struct _block_head* bh, long max_count)
{
	long count = 0;
	long block_size = bh->_block_size;

	struct _free_list_node* fln;
	struct _free_list_head* flh;
	struct _block_head* nbh = _next_block(bh);

	if(nbh->_flag & BLOCK_BIT_USED) goto succ_ret;

	_unlink_block(mmpi, bh);

	while(count < max_count && block_size < _block_size(mmpi->_cfg._max_block_idx))
	{
		struct _block_head* nbh = _next_block(bh);
		if(nbh->_flag != 0) break;

		_unlink_block(mmpi, nbh);
		block_size += nbh->_block_size;
	}

	if(!is_2power(block_size)) goto error_ret;

	_make_block(bh, block_size);

succ_ret:
	return count;
error_ret:
	return -1;
}

static struct _block_head* _merge_unmblock(struct _mmpool_impl* mmpi, struct _block_head* bh)
{
	long block_size;
	struct _block_head* nbh;

	if((bh->_flag & BLOCK_BIT_UNM) == 0)
		goto error_ret;
	if((bh->_flag & BLOCK_BIT_USED) != 0)
		goto error_ret;

	nbh = _next_block(bh);
	if((nbh->_flag & BLOCK_BIT_UNM_SUC) == 0)
		goto error_ret;
	if((nbh->_flag & BLOCK_BIT_USED) != 0)
		goto error_ret;

	_unlink_block(mmpi, nbh);

	block_size = bh->_block_size + nbh->_block_size;
	if(!is_2power(block_size))
		goto error_ret;

	bh = _make_block(bh, block_size);

	return bh;
error_ret:
	printf("merge unmblock failed.\n");
	return 0;
}

static struct _block_head* _merge_to_unmblock(struct _mmpool_impl* mmpi, struct _block_head* bh)
{
	long block_size;
	long unm_used = 0;
	struct _block_head* nbh;

	if((bh->_flag & BLOCK_BIT_UNM) == 0)
		goto error_ret;

	if(bh->_flag & BLOCK_BIT_USED)
		unm_used = 1;

	nbh = _next_block(bh);
	if((nbh->_flag & BLOCK_BIT_UNM_SUC) == 0)
		goto error_ret;
	if((nbh->_flag & BLOCK_BIT_USED) != 0)
		goto error_ret;

	_unlink_block(mmpi, nbh);

	block_size = bh->_block_size + nbh->_block_size;
	if(!is_2power(block_size))
		goto error_ret;

	bh = _make_block(bh, block_size);
	if(unm_used)
		bh->_flag |= BLOCK_BIT_USED;

	return bh;
error_ret:
	printf("merge unmblock failed.\n");
	return 0;
}

static long  _mmp_init_block(struct _mmpool_impl* mmpi, void* blk, long head_idx)
{
	struct _block_head* hd = _make_block(blk, _block_size_by_idx(mmpi, head_idx));
	if(!hd) goto error_ret;

	return _return_free_node_to_head(mmpi, hd);
error_ret:
	return -1;
}

static long _mmp_init_chunk(struct _mmpool_impl* mmpi)
{
	void* chk1;
	void* chk2 = 0;
	void* end;
	long blk_size;
	long rslt = -1;

	struct _chunk_head* hd = (struct _chunk_head*)(mmpi->_chunk_addr - sizeof(struct _chunk_head));
	if(!hd) goto error_ret;

	hd->_chunck_label = CHUNK_LABEL;
	hd->_reserved = 0;
	end = mmpi->_chunk_addr + mmpi->_chunk_size;
	chk1 = mmpi->_chunk_addr;

	for(long idx = mmpi->_cfg._free_list_count - 1; idx > 0 && chk1 < end && chk2 < end; idx >>= 1)
	{
		blk_size = _block_size_by_idx(mmpi, idx);

		chk2 = chk1 + blk_size;

		while(chk2 <= end)
		{
			rslt = _mmp_init_block(mmpi, chk1, idx);
			if(rslt < 0) goto error_ret;

			chk1 = chk2;
			chk2 = chk1 + blk_size;
		}
	}

	return rslt;
error_ret:
	return -1;
}

static long _mmp_load_chunk(struct _mmpool_impl* mmpi)
{
	void* chk;
	void* end;
	long blk_size;
	long payload_size;
	long rslt = 0;
	struct _block_head* bhd;

	struct _chunk_head* chd = (struct _chunk_head*)(mmpi->_chunk_addr - sizeof(struct _chunk_head));
	if(!chd) goto error_ret;

	if(chd->_chunck_label != CHUNK_LABEL) goto error_ret;

	chk = mmpi->_chunk_addr;
	end = mmpi->_chunk_addr + mmpi->_chunk_size;

	while(chk <= end)
	{
		bhd = (struct _block_head*)chk;
		if(bhd->_flag & BLOCK_BIT_USED) continue;

		payload_size = _payload_size(bhd->_block_size);

		rslt = _return_free_node_to_head(mmpi, bhd);
		if(rslt < 0) goto error_ret;
	}

	return rslt;
error_ret:
	return -1;
}


struct mmpool* mmp_new(void* addr, long size, struct mmpool_config* cfg)
{
	struct _chunk_head* ch = 0;
	struct _mmpool_impl* mmpi = 0;
	long result = 0;

	if(!addr) goto error_ret;

	// chunk must be 8-byte aligned.
	if(((unsigned long)addr) & 0x7 != 0) goto error_ret;
	if(size < _block_size(cfg->min_block_index)) goto error_ret;
	if(size > 0xFFFFFFFF) goto error_ret;

	mmpi = malloc(sizeof(struct _mmpool_impl));
	if(!mmpi) goto error_ret;

	mmpi->_cfg._min_block_idx = cfg->min_block_index;
	mmpi->_cfg._max_block_idx = cfg->max_block_index;
	mmpi->_cfg._free_list_count = cfg->max_block_index - cfg->min_block_index + 1;
	mmpi->_cfg._min_payload_size = cfg->min_block_index + HEAD_TAIL_SIZE;
	mmpi->_cfg._max_payload_size = cfg->max_block_index + HEAD_TAIL_SIZE;

	mmpi->_fln_count = size / _block_size(cfg->min_block_index);
	mmpi->_fln_pool = malloc(mmpi->_fln_count * sizeof(struct _free_list_node));
	if(!mmpi->_fln_pool) goto error_ret;

	mmpi->_flh = malloc(mmpi->_cfg._free_list_count * sizeof(struct _free_list_head));
	if(!mmpi->_flh) goto error_ret;

	lst_new(&mmpi->_free_fln_list);

	for(long i = 0; i < mmpi->_fln_count; ++i)
	{
		lst_clr(&mmpi->_fln_pool[i]._free_fln_node);
		mmpi->_fln_pool[i]._block = 0;
		mmpi->_fln_pool[i]._idx = i;
		lst_push_back(&mmpi->_free_fln_list, &mmpi->_fln_pool[i]._free_fln_node);
	}

	for(long i = 0; i < mmpi->_cfg._free_list_count; ++i)
	{
		mmpi->_flh[i]._op_count = 0;
		lst_new(&mmpi->_flh[i]._free_list);
	}

	mmpi->_pool.mm_addr = addr;
	mmpi->_pool.mm_size = size;


	mmpi->_chunk_addr = addr + sizeof(struct _chunk_head) + PAGE_SIZE;
	mmpi->_chunk_addr = (void*)((unsigned long)mmpi->_chunk_addr & (unsigned long)(-PAGE_SIZE));

	if(mmpi->_chunk_addr < addr + sizeof(struct _chunk_head)) goto error_ret;

	mmpi->_chunk_size = (size - (mmpi->_chunk_addr - mmpi->_pool.mm_addr)) & (-_block_size(mmpi->_cfg._max_block_idx));
	if(mmpi->_chunk_size <= 0) goto error_ret;

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
		if(mmpi->_flh)
			free(mmpi->_flh);

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
		if(mmpi->_flh)
			free(mmpi->_flh);

		free(mmpi);
	}
}

void* mmp_alloc(struct mmpool* mmp, long payload_size)
{
	long rslt = 0;
	long flh_idx = 0;
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;
	struct _block_head* bh = 0;

	if(payload_size <= 0 || payload_size > mmpi->_cfg._max_payload_size) goto error_ret;

	flh_idx = _flh_idx(mmpi, payload_size);
	if(flh_idx < 0) goto error_ret;

	do
	{
		rslt = _take_free_node(mmpi, flh_idx, &bh);
		if(rslt < 0) goto error_ret;
	}
	while(bh == 0);

	rslt = _try_spare_block(mmpi, bh, payload_size);
	if(rslt < 0) goto error_ret;

	bh->_flag |= BLOCK_BIT_USED;

	return _payload_addr(bh);
error_ret:
	return 0;
}

long mmp_free(struct mmpool* mmp, void* p)
{
	long rslt = -1;
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;
	struct _block_head* bh;
	struct _block_tail* tl;

	if(!p) goto error_ret;

	bh = (struct _block_head*)(p - sizeof(struct _block_head));

	tl = _get_tail(bh);

	if(tl->_block_size != bh->_block_size || tl->_tail_label != TAIL_LABEL)
		goto error_ret;
	
	if((bh->_flag & BLOCK_BIT_USED) == 0) goto error_ret;
	bh->_flag ^= BLOCK_BIT_USED;

	if(bh->_flag & BLOCK_BIT_UNM)
	{
		bh = _merge_unmblock(mmpi, bh);
		if(!bh) goto succ_ret;
	}
	else if(bh->_flag & BLOCK_BIT_UNM_SUC)
	{
		bh = _merge_to_unmblock(mmpi, _prev_block(bh));
		if(!bh) goto succ_ret;
	}

	if(bh->_flag == 0)
		rslt = _return_free_node_to_head(mmpi, bh);

	return rslt;
succ_ret:
	return 0;
error_ret:
	return -1;
}

long mmp_check(struct mmpool* mmp)
{
	long sum_list_size = 0;
	struct _block_head* h = 0;
	struct _mmpool_impl* mmpi = (struct _mmpool_impl*)mmp;

	h = (struct _block_head*)(mmpi->_chunk_addr);

	while((void*)h < (void*)mmpi->_chunk_addr + mmpi->_chunk_size)
	{
		if(h->_flag != 0)
			printf("flag: %d, block size: %d.\n", h->_flag, h->_block_size);

		printf("block size: %u, idx: %d.\n", h->_block_size, h->_fln_idx);

		h = _next_block(h);
	}

//	for(long i = 0; i < FREE_LIST_COUNT; ++i)
//	{
//		sum_list_size += mmpi->_flh[i]._free_list.size;
//	}
//
//	if(sum_list_size + mmpi->_free_fln_list.size != mmpi->_fln_count) goto error_ret;

	for(long i = 0; i < mmpi->_cfg._free_list_count; ++i)
	{
		lst_check(&mmpi->_flh[i]._free_list);
		printf("list[%ld]: node count[%ld], op_count[%ld].\n", i,
				mmpi->_flh[i]._free_list.size, mmpi->_flh[i]._op_count);
	}

	lst_check(&mmpi->_free_fln_list);

	return 0;
error_ret:
	return -1;
}

