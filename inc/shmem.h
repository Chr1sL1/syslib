#ifndef __shmem_h__
#define __shmem_h__

#include "rbtree.h"
#include "dlist.h"

#define MAX_SHMM_NAME_LEN (255)

typedef void* (*shmm_alloc_func)(unsigned long);
typedef long (*shmm_free_func)(void*);

struct shmm_blk
{
	void* addr_begin;
	void* addr_end;

	struct rbnode rb_node;
	struct dlnode lst_node;
};

struct shmm_blk* shmm_create(int key, void* at_addr, unsigned long size, int try_huge_page);
struct shmm_blk* shmm_open(int key, void* at_addr);
struct shmm_blk* shmm_open_raw(int key, void* at_addr);

long shmm_close(struct shmm_blk* shm);
long shmm_destroy(struct shmm_blk* shm);

#endif
