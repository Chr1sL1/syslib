#include "shmem.h"
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SHMM_MID_PAGE_THRESHOLD		(1 << 18)
#define SHMM_HUGE_PAGE_THRESHOLD	(1 << 28)

#define SHMM_TAG					(0x1234567890abcdef)

#define SHM_PAGE_SHIFT				(12)
#define SHM_PAGE_SIZE				(1 << SHM_PAGE_SHIFT)

#define SHM_HUGE_SHIFT				(26)
#define SHM_HUGE_2MB				(21 << SHM_HUGE_SHIFT)
#define SHM_HUGE_1GB				(30 << SHM_HUGE_SHIFT)

#pragma pack(1)
struct _shmm_blk_head
{
	unsigned long _shmm_tag;
	unsigned long _addr_begin;
	unsigned long _addr_end;
	long _the_key;
};
#pragma pack()

struct _shmm_blk_impl
{
	struct shmm_blk _the_blk;
	int _fd;
};

static inline struct _shmm_blk_impl* _conv_blk(struct shmm_blk* blk)
{
	return (struct _shmm_blk_impl*)((unsigned long)blk - (unsigned long)&(((struct _shmm_blk_impl*)(0))->_the_blk));
}

struct shmm_blk* shmm_create(const char* shmm_name, long channel, unsigned long size, int try_huge_page)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_head* sbh;
	struct _shmm_blk_impl* sbi;
	long name_len;

	if(shmm_name == 0) goto error_ret;

	name_len = strlen(shmm_name);

	if(name_len <= 0 || name_len > MAX_SHMM_NAME_LEN || channel <= 0 || size <= 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(!sbi) goto error_ret;

	sbi->_the_blk.key = ftok(shmm_name, channel);
	if(sbi->_the_blk.key < 0) goto error_ret;

	flag |= IPC_CREAT;
	flag |= IPC_EXCL;
	flag |= SHM_R;
	flag |= SHM_W;

#ifdef __linux__
	if(try_huge_page)
	{
		if(size > SHMM_MID_PAGE_THRESHOLD)
		{
			flag |= SHM_HUGETLB;
			if(size <= SHMM_HUGE_PAGE_THRESHOLD)
				flag |= SHM_HUGE_2MB;
			else
				flag |= SHM_HUGE_1GB;
		}
	}
#endif

	sbi->_fd = shmget(sbi->_the_blk.key, size + sizeof(struct _shmm_blk_head), flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, 0, SHM_RND);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	sbh->_shmm_tag = SHMM_TAG;
	sbh->_addr_begin = (unsigned long)ret_addr;
	sbh->_addr_end = (unsigned long)ret_addr + size + sizeof(struct _shmm_blk_head);
	sbh->_the_key = sbi->_the_blk.key;

	sbi->_the_blk.addr_begin = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.addr_end = (void*)sbh->_addr_end;

	return &sbi->_the_blk;
error_ret:
	if(sbi)
	{
		if(ret_addr)
			shmdt(ret_addr);
		free(sbi);
	}
	return 0;
}

struct shmm_blk* shmm_open(const char* shmm_name, long channel, void* at_addr)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_impl* sbi;
	struct _shmm_blk_head* sbh;
	long name_len;

	if(shmm_name == 0) goto error_ret;

	name_len = strlen(shmm_name);

	if(name_len <= 0 || name_len > MAX_SHMM_NAME_LEN || channel <= 0)
		goto error_ret;

	if(at_addr)
		at_addr -= sizeof(struct _shmm_blk_head);

	if(at_addr && ((unsigned long)at_addr & ~(SHM_PAGE_SIZE - 1)) != 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(sbi == 0) goto error_ret;


	sbi->_the_blk.key = ftok(shmm_name, channel);
	if(sbi->_the_blk.key < 0) goto error_ret;

	flag |= SHM_R;
	flag |= SHM_W;

	sbi->_fd = shmget(sbi->_the_blk.key, 0, flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, at_addr, 0);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	if(sbh->_shmm_tag != SHMM_TAG || sbh->_addr_begin != (unsigned long)ret_addr || sbi->_the_blk.key != sbh->_the_key)
		goto error_ret;

	sbi->_the_blk.addr_begin = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.addr_end = sbi->_the_blk.addr_end;

	return &sbi->_the_blk;
error_ret:
	if(sbi)
	{
		if(ret_addr)
			shmdt(ret_addr);

		free(sbi);
	}
	return 0;
}

struct shmm_blk* shmm_create_key(long key, unsigned long size, int try_huge_page)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_head* sbh;
	struct _shmm_blk_impl* sbi;

	if(key == IPC_PRIVATE || key <= 0 || size <= 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(!sbi) goto error_ret;

	sbi->_the_blk.key = key;

	flag |= IPC_CREAT;
	flag |= IPC_EXCL;
	flag |= SHM_R;
	flag |= SHM_W;

#ifdef __linux__
	if(try_huge_page)
	{
		if(size > SHMM_MID_PAGE_THRESHOLD)
		{
			flag |= SHM_HUGETLB;
			if(size <= SHMM_HUGE_PAGE_THRESHOLD)
				flag |= SHM_HUGE_2MB;
			else
				flag |= SHM_HUGE_1GB;
		}
	}
#endif

	sbi->_fd = shmget(sbi->_the_blk.key, size + sizeof(struct _shmm_blk_head), flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, 0, SHM_RND);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	sbh->_shmm_tag = SHMM_TAG;
	sbh->_addr_begin = (unsigned long)ret_addr;
	sbh->_addr_end = (unsigned long)ret_addr + size + sizeof(struct _shmm_blk_head);
	sbh->_the_key = sbi->_the_blk.key;

	sbi->_the_blk.addr_begin = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.addr_end = (void*)sbh->_addr_end;

	return &sbi->_the_blk;
error_ret:
	if(sbi)
	{
		if(ret_addr)
			shmdt(ret_addr);
		free(sbi);
	}
	return 0;

}

struct shmm_blk* shmm_open_key(long key, void* at_addr)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_impl* sbi;
	struct _shmm_blk_head* sbh;

	if(key == IPC_PRIVATE || key <= 0)
		goto error_ret;

	if(at_addr)
		at_addr -= sizeof(struct _shmm_blk_head);

	if(at_addr && ((unsigned long)at_addr & ~(SHM_PAGE_SIZE - 1)) != 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(sbi == 0) goto error_ret;

	sbi->_the_blk.key = key;

	flag |= SHM_R;
	flag |= SHM_W;

	sbi->_fd = shmget(sbi->_the_blk.key, 0, flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, at_addr, 0);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	if(sbh->_shmm_tag != SHMM_TAG || sbh->_addr_begin != (unsigned long)ret_addr || sbi->_the_blk.key != sbh->_the_key)
		goto error_ret;

	sbi->_the_blk.addr_begin = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.addr_end = sbi->_the_blk.addr_end;

	return &sbi->_the_blk;
error_ret:
	if(sbi)
	{
		if(ret_addr)
			shmdt(ret_addr);
		free(sbi);
	}
	return 0;

}

long shmm_close(struct shmm_blk** shmb)
{
	struct _shmm_blk_head* hd;
	struct _shmm_blk_impl* sbi = _conv_blk(*shmb);
	if(!sbi->_the_blk.addr_begin || !sbi->_the_blk.addr_end)
		goto error_ret;

	hd = (struct _shmm_blk_head*)(sbi->_the_blk.addr_begin - sizeof(struct _shmm_blk_head));
	if(hd->_shmm_tag != SHMM_TAG)
		goto error_ret;

	shmdt(hd);

	free(sbi);
	*shmb = 0;

	return 0;
error_ret:
	if(sbi)
		free(sbi);
	*shmb = 0;
	return -1;
}

long shmm_destroy(struct shmm_blk** shmb)
{
	long rslt;
	void* ret_addr;
	struct _shmm_blk_impl* sbi = _conv_blk(*shmb);
	if(shmm_close(shmb) < 0)
		goto error_ret;

	rslt = shmctl(sbi->_fd, IPC_RMID, 0);
	if(rslt < 0)
		goto error_ret;

	free(sbi);
	*shmb = 0;

	return 0;
error_ret:
	if(sbi)
		free(sbi);
	*shmb = 0;
	return -1;
}
