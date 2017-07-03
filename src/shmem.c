#include "shmem.h"
#include <string.h>
#include <stdlib.h>
#include <sys/shm.h>

#define SHMM_MID_PAGE_THRESHOLD		(1 << 18)
#define SHMM_HUGE_PAGE_THRESHOLD	(1 << 28)

#define SHMM_TAG					(0x1234567890abcdef)

#define SHM_HUGE_SHIFT				(26)
#define SHM_HUGE_2MB				(21 << SHM_HUGE_SHIFT)
#define SHM_HUGE_1GB				(30 << SHM_HUGE_SHIFT)

#pragma pack(1)
struct _shmm_blk_head
{
	unsigned long _shmm_tag;
	long _shmm_size;
};
#pragma pack()

struct _shmm_blk_impl
{
	struct shmm_blk _the_blk;
	long _the_key;
	long _fd;
};

static inline struct _shmm_blk_impl* _conv_blk(struct shmm_blk* blk)
{
	return (struct _shmm_blk_impl*)((unsigned long)blk - (unsigned long)&(((struct _shmm_blk_impl*)(0))->_the_blk));
}

struct shmm_blk* shmm_new(const char* shmm_name, long channel, long size, long try_huge_page)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_head* sbh;
	struct _shmm_blk_impl* sbi;
	if(shmm_name == 0 || strlen(shmm_name) == 0 || channel <= 0 || size <= 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(sbi == 0) goto error_ret;

	sbi->_the_key = ftok(shmm_name, channel);
	if(sbi->_the_key < 0) goto error_ret;

	flag |= IPC_CREAT;
	flag |= IPC_EXCL;
	flag |= SHM_R;
	flag |= SHM_W;

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

	sbi->_fd = shmget(sbi->_the_key, size + sizeof(struct _shmm_blk_head), flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, 0, SHM_RND);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	sbh->_shmm_tag = SHMM_TAG;
	sbh->_shmm_size = size;

	sbi->_the_blk.addr = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.size = size;

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

static long _shmm_get_size(const char* shmm_name, long channel)
{
	int flag = 0;
	int fd;
	long key;
	long size;
	struct _shmm_blk_head* hd;

	if(shmm_name == 0 || strlen(shmm_name) == 0 || channel <= 0)
		goto error_ret;

	key = ftok(shmm_name, channel);
	if(key < 0) goto error_ret;

	flag |= SHM_R;

	fd = shmget(key, sizeof(struct _shmm_blk_head), flag);
	if(fd < 0)
		goto error_ret;

	hd = (struct _shmm_blk_head*)shmat(fd, 0, SHM_RND);
	if(hd == (void*)(-1) || hd->_shmm_tag != SHMM_TAG)
		goto error_ret;

	size = hd->_shmm_size;

	shmdt(hd);

	return size;
error_ret:
	return -1;
}

struct shmm_blk* shmm_open(const char* shmm_name, long channel)
{
	int flag = 0;
	void* ret_addr = 0;
	struct _shmm_blk_impl* sbi;
	struct _shmm_blk_head* sbh;

	if(shmm_name == 0 || strlen(shmm_name) == 0 || channel <= 0)
		goto error_ret;

	sbi = malloc(sizeof(struct _shmm_blk_impl));
	if(sbi == 0) goto error_ret;

	sbi->_the_key = ftok(shmm_name, channel);
	if(sbi->_the_key < 0) goto error_ret;

	flag |= SHM_R;
	flag |= SHM_W;

	sbi->_fd = shmget(sbi->_the_key, 0, flag);
	if(sbi->_fd < 0)
		goto error_ret;

	ret_addr = shmat(sbi->_fd, 0, SHM_RND);
	if(ret_addr == (void*)(-1))
		goto error_ret;

	sbh = (struct _shmm_blk_head*)ret_addr;
	if(sbh->_shmm_tag != SHMM_TAG)
		goto error_ret;

	sbi->_the_blk.addr = ret_addr + sizeof(struct _shmm_blk_head);
	sbi->_the_blk.size = sbh->_shmm_size;

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
	if(sbi->_the_blk.addr == 0 || sbi->_the_blk.size == 0)
		goto error_ret;

	hd = (struct _shmm_blk_head*)(sbi->_the_blk.addr - sizeof(struct _shmm_blk_head));
	if(hd->_shmm_tag != SHMM_TAG || hd->_shmm_size <= 0)
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

long shmm_del(struct shmm_blk** shmb)
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
