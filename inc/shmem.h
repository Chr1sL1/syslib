#ifndef __shmem_h__
#define __shmem_h__

#define SHMEM_NAME_LEN	(64)

struct _shm_addr
{
	void* _addr;
	size_t _size;
};

struct shm_host_ptr
{
	struct _shm_addr _the_addr;
	int _fd;
	char name[SHMEM_NAME_LEN];
};

struct shm_client_ptr
{
	struct _shm_addr _the_addr;
	int _fd;
};

struct shm_host_ptr* shmem_new(const char* name, size_t size);
void shmem_destroy(struct shm_host_ptr* ptr);

struct shm_client_ptr* shmem_open(const char* name);
void shmem_close(struct shm_client_ptr* ptr);

// hahaha
#endif