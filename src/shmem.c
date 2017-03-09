#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "shmem.h"

struct shm_host_ptr* shmem_new(const char* name, size_t size)
{
	int __fd = 0;
	int __test_fd = 0;
	struct shm_host_ptr* __ptr = NULL;

	if(strlen(name) > SHMEM_NAME_LEN - 1)
		goto error_end;

	__test_fd = shm_open(name, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__test_fd > 0)
		goto error_end;

	__fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__fd < 0)
		goto error_end;
		
	if(ftruncate(__fd, size + sizeof(struct _shm_addr)) < 0)
		goto error_end;
	
	__ptr = malloc(sizeof(struct shm_host_ptr));
	if(!__ptr)
		goto error_end;

	memset(__ptr, 0, sizeof(struct shm_host_ptr));
	strncpy(__ptr->name, name, sizeof(__ptr->name));

	__ptr->_the_addr._addr = mmap(NULL, size + sizeof(struct _shm_addr),
							PROT_READ | PROT_WRITE, MAP_SHARED, __fd, 0);

	if(__ptr->_the_addr._addr == MAP_FAILED)
		goto error_end;
	
	__ptr->_the_addr._size = size;

	if(write(__fd, __ptr, sizeof(struct _shm_addr)) != sizeof(struct _shm_addr))
		goto error_end;
	
	__ptr->_the_addr._addr += sizeof(struct _shm_addr);

	return __ptr;

error_end:
	perror("shmem_new error occured.");
	if(__ptr)
	{
		shmem_destroy(__ptr);
	}
	else if(__fd > 0)
	{
		close(__fd);
	}

	return NULL; 
}

void shmem_destroy(struct shm_host_ptr* ptr)
{
	if(ptr == NULL)
		goto error_end;

	shm_unlink(ptr->name);

	if(ptr->_the_addr._addr == NULL)
		goto error_end;

	if(munmap(ptr->_the_addr._addr - sizeof(struct _shm_addr), ptr->_the_addr._size + sizeof(struct _shm_addr)) < 0)
		perror("munmap error.");

	if(ptr->_fd > 0)
		close(ptr->_fd);

	free(ptr);

error_end:
	return;
}

struct shm_client_ptr* shmem_open(const char* name)
{
	int __fd = 0;
	struct shm_client_ptr* __ptr = NULL;

	__fd = shm_open(name, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__fd < 0)
		goto error_end;

	__ptr = malloc(sizeof(struct shm_client_ptr));
	if(!__ptr)
		goto error_end;

	memset(__ptr, 0, sizeof(struct shm_client_ptr));

	if(read(__fd, __ptr, sizeof(struct _shm_addr)) != sizeof(struct _shm_addr))
		goto error_end;

	__ptr->_the_addr._addr = mmap(__ptr->_the_addr._addr - sizeof(struct _shm_addr),
							__ptr->_the_addr._size + sizeof(struct _shm_addr),
							PROT_READ | PROT_WRITE, MAP_SHARED, __fd, 0);

	if(__ptr->_the_addr._addr == MAP_FAILED)
		goto error_end;

	__ptr->_the_addr._addr += sizeof(struct _shm_addr);

	return __ptr;

error_end:
	perror("shmem_open error occured.");

	if(__ptr)
	{
		shmem_close(__ptr);
	}
	else if(__fd > 0)
	{
		close(__fd);
	}

	return NULL;
}

void shmem_close(struct shm_client_ptr* ptr)
{
	if(ptr == NULL)
		goto error_end;

	if(ptr->_the_addr._addr == NULL)
		goto error_end;

	if(munmap(ptr->_the_addr._addr - sizeof(struct _shm_addr), ptr->_the_addr._size + sizeof(struct _shm_addr)) < 0)
		perror("munmap error.");

	if(ptr->_fd > 0)
		close(ptr->_fd);

	free(ptr);

error_end:
	return;
}