#include "mmspace.h"
#include "shmem.h"
#include "mmzone.h"
#include "dlist.h"
#include "rbtree.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MAX_CHUNK_COUNT (1024)
#define MM_INF_FILE_VER (1)

struct _mm_chunk_impl
{
	struct mm_chunk _the_chunk;
	struct dlnode _list_node;
	struct rbnode _rb_node;

	struct dlist _zone_list[MMZ_COUNT];
	struct rbtree _zone_tree;
};

struct _mm_chunk_save_inf
{
	char _chunk_name[MAX_SHMM_NAME_LEN];
	int _channel;
	unsigned long _map_addr;
};

struct _mm_space_impl
{
	struct dlist _chunk_list;
	struct rbtree _chunk_tree;

	long _chunk_save_count;
	struct _mm_chunk_save_inf _chunk_save_inf[MAX_CHUNK_COUNT];
};

#pragma pack(1)

struct _mm_inf_file_header
{
	unsigned long _file_tag;
	unsigned int _file_version;
	unsigned int _chunk_count;
};

struct _mm_inf_chunk_dat
{
	char _chunk_name[MAX_SHMM_NAME_LEN];
	int _channel;
	unsigned long _map_addr;
};

#pragma pack()

#define MM_INF_FILE_TAG (0x6368756E6B696E66UL)

static struct _mm_space_impl* __the_mmspace = 0;

static inline long _mm_link_chunk(struct _mm_chunk_impl* mci, const char* name, int channel, void* at_addr);

static struct _mm_chunk_impl* _conv_chunk(struct mm_chunk* mmc)
{
	return (struct _mm_chunk_impl*)((unsigned long)mmc - (unsigned long)(&(((struct _mm_chunk_impl*)0)->_the_chunk)));
}

static struct _mm_chunk_impl* _conv_rbnode(struct rbnode* n)
{
	return (struct _mm_chunk_impl*)((unsigned long)n - (unsigned long)(&(((struct _mm_chunk_impl*)0)->_rb_node)));
}

extern long _comp_zone_node(void* key, struct rbnode* n);

static long _comp_chunk_node(void* key, struct rbnode* n)
{
	struct _mm_chunk_impl* mci = _conv_rbnode(n);

	if(key < mci->_the_chunk.shm_blk->addr_begin)
		return -1;
	else if(key < mci->_the_chunk.shm_blk->addr_end)
		return 0;

	return 1;
}

static inline long _mmc_link_zone(struct _mm_chunk_impl* mci, struct mm_zone* zone)
{
	long rslt;

	if(zone->type <= MMZ_INVALID || zone->type >= MMZ_COUNT)
		goto error_ret;

	rslt = rb_insert(&mci->_zone_tree, mmz_rbnode(zone));
	if(rslt < 0) goto error_ret;

	rslt = lst_push_back(&mci->_zone_list[zone->type], mmz_lstnode(zone));
	if(rslt < 0) goto error_ret;

	return 0;
error_ret:
	return -1;
}

struct mm_chunk* mm_chunk_create(const char* name, int channel, unsigned long size, int try_huge_page)
{
	long rslt;
	struct _mm_chunk_impl* mci;

	if(!__the_mmspace) goto error_ret;

	mci = malloc(sizeof(struct _mm_chunk_impl));
	if(!mci) goto error_ret;

	mci->_the_chunk.shm_blk = shmm_create(name, channel, size, try_huge_page);
	if(!mci->_the_chunk.shm_blk)
		goto error_ret;

	for(int i = MMZ_BEGIN; i < MMZ_COUNT; ++i)
	{
		lst_new(&mci->_zone_list[i]);
	}

	rb_init(&mci->_zone_tree, _comp_zone_node);

	lst_clr(&mci->_list_node);
	rb_fillnew(&mci->_rb_node);

	mci->_the_chunk.remain_size = mci->_the_chunk.shm_blk->addr_end - mci->_the_chunk.shm_blk->addr_begin;

	rslt = _mm_link_chunk(mci, name, channel, mci->_the_chunk.shm_blk->addr_begin);
	if(rslt < 0) goto error_ret;

	return &mci->_the_chunk;
error_ret:
	if(mci)
	{
		free(mci);
	}
	return 0;
}

struct mm_chunk* mm_chunk_load(const char* name, int channel, void* at_addr)
{
	long rslt;
	void* load_pos;
	struct _mm_chunk_impl* mci;
	struct mm_zone* zone;

	if(!__the_mmspace) goto error_ret;

	mci = malloc(sizeof(struct _mm_chunk_impl));
	if(!mci) goto error_ret;

	mci->_the_chunk.shm_blk = shmm_open(name, channel, at_addr);
	if(!mci->_the_chunk.shm_blk)
		goto error_ret;

	load_pos = mci->_the_chunk.shm_blk->addr_begin;

	while(load_pos <= mci->_the_chunk.shm_blk->addr_end)
	{
		zone = mmz_load(load_pos);
		if(!zone) break;

		rslt = _mmc_link_zone(mci, zone);
		if(rslt < 0) break;

		load_pos = zone->addr_end;
	}

	rslt = _mm_link_chunk(mci, name, channel, mci->_the_chunk.shm_blk->addr_begin);
	if(rslt < 0) goto error_ret;

	return &mci->_the_chunk;

error_ret:
	if(mci)
	{
		free(mci);
	}
	return 0;
}

long mm_chunk_destroy(struct mm_chunk* mmc)
{

error_ret:
	return -1;
}

long mm_chunk_add_zone(struct mm_chunk* mmc, long type, unsigned long size, struct mm_zone_config* cfg)
{
	long rslt;
	void* zone_begin;
	void* zone_end;
	struct mm_zone* zone;

	struct _mm_chunk_impl* mci = _conv_chunk(mmc);
	if(!mci) goto error_ret;

	if(size > mci->_the_chunk.remain_size)
		goto error_ret;

	zone_begin = mci->_the_chunk.shm_blk->addr_end - mci->_the_chunk.remain_size;
	zone_end = zone_begin + size;

	zone = mmz_create(type, zone_begin, zone_end, cfg);
	if(!zone) goto error_ret;

	rslt = _mmc_link_zone(mci, zone);
	if(rslt < 0) goto error_ret;

	mci->_the_chunk.remain_size -= size;

	return 0;
error_ret:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
// mm functions:
//
static inline long _mm_link_chunk(struct _mm_chunk_impl* mci, const char* name, int channel, void* at_addr)
{
	long rslt;

	rslt = lst_push_back(&__the_mmspace->_chunk_list, &mci->_list_node);
	if(rslt < 0) goto error_ret;

	rslt = rb_insert(&__the_mmspace->_chunk_tree, &mci->_rb_node);
	if(rslt < 0) goto error_ret;

	__the_mmspace->_chunk_save_inf[__the_mmspace->_chunk_save_count]._channel = channel;
	__the_mmspace->_chunk_save_inf[__the_mmspace->_chunk_save_count]._map_addr = (unsigned long)at_addr;
	strncpy(__the_mmspace->_chunk_save_inf[__the_mmspace->_chunk_save_count]._chunk_name, name, MAX_SHMM_NAME_LEN - 1);

	++__the_mmspace->_chunk_save_count;

	return 0;
error_ret:
	return -1;
}

long mm_initialize(void)
{
	if(__the_mmspace) goto error_ret;

	__the_mmspace = malloc(sizeof(struct _mm_space_impl));
	if(!__the_mmspace) goto error_ret;

	lst_new(&__the_mmspace->_chunk_list);
	rb_init(&__the_mmspace->_chunk_tree, _comp_chunk_node);

	__the_mmspace->_chunk_save_count = 0;

	return 0;
error_ret:
	return -1;
}

long mm_load(const char* mm_inf_file)
{
	int fd;
	long rslt;
	struct _mm_inf_file_header hd;

	if(!__the_mmspace) goto error_ret;

	if(__the_mmspace->_chunk_save_count > 0)
		goto error_ret;

	fd = open(mm_inf_file, O_RDONLY);
	if(fd < 0) goto error_ret;

	rslt = read(fd, &hd, sizeof(struct _mm_inf_file_header));
	if(rslt < sizeof(struct _mm_inf_file_header))
		goto error_ret;

	if(hd._file_tag != MM_INF_FILE_TAG || hd._file_version != MM_INF_FILE_VER || hd._chunk_count > MAX_CHUNK_COUNT)
		goto error_ret;

	for(int i = 0; i < hd._chunk_count; ++i)
	{
		struct _mm_inf_chunk_dat dat;
		struct mm_chunk* mmc; 

		rslt = read(fd, &dat, sizeof(struct _mm_inf_chunk_dat));
		if(rslt < sizeof(struct _mm_inf_chunk_dat))
			goto error_ret;

		mmc = mm_chunk_load(dat._chunk_name, dat._channel, (void*)dat._map_addr);
		if(!mmc) goto error_ret;
	}

	return 0;
error_ret:
	if(fd > 0)
		close(fd);
	return -1;
}

long mm_save(const char* mm_inf_file)
{
	int fd;
	long rslt;
	struct _mm_inf_file_header hd;

	if(!__the_mmspace) goto error_ret;

	fd = open(mm_inf_file, O_CREAT | O_WRONLY | O_TRUNC);
	if(fd < 0) goto error_ret;

	hd._file_tag = MM_INF_FILE_TAG;
	hd._file_version = MM_INF_FILE_VER;
	hd._chunk_count = __the_mmspace->_chunk_save_count;

	rslt = write(fd, &hd, sizeof(struct _mm_inf_file_header));
	if(rslt < sizeof(struct _mm_inf_file_header))
		goto error_ret;

	for(int i = 0; i < __the_mmspace->_chunk_save_count; ++i)
	{
		struct _mm_inf_chunk_dat dat;
		dat._map_addr = __the_mmspace->_chunk_save_inf[i]._map_addr;
		dat._channel = __the_mmspace->_chunk_save_inf[i]._channel;
		memcpy(dat._chunk_name, __the_mmspace->_chunk_save_inf[i]._chunk_name, MAX_SHMM_NAME_LEN);

		rslt = write(fd, &dat, sizeof(struct _mm_inf_chunk_dat));
		if(rslt < sizeof(struct _mm_inf_chunk_dat))
			goto error_ret;
	}

	close(fd);

	return 0;
error_ret:
	if(fd > 0)
		close(fd);
	return -1;
}

long mm_uninitialize(void)
{
	if(!__the_mmspace) goto error_ret;

error_ret:
	return -1;
}

void* mm_alloc(unsigned long size, int alloc_flag)
{
	if(!__the_mmspace) goto error_ret;
error_ret:
	return 0;
}

long mm_free(void* p)
{
	if(!__the_mmspace) goto error_ret;

error_ret:
	return -1;
}

