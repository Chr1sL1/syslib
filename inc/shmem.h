#ifndef __shmem_h__
#define __shmem_h__

#define SHMEM_NAME_LEN	(64)

struct shm_ptr
{
	void* addr;
	size_t size;
	char name[SHMEM_NAME_LEN];
};

struct shm_ptr* shmem_new(const char* name, size_t size);
struct shm_ptr* shmem_open(const char* name);
void shmem_close(struct shm_ptr* ptr);

// hahaha
#endif