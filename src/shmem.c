#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "shmem.h"

struct shm_ptr* shmem_new(const char* name, size_t size)
{
	int __fd = 0;
	int __test_fd = 0;
	struct shm_ptr* __ptr = NULL;
	long page_size = sysconf(_SC_PAGESIZE);

	if(strlen(name) > SHMEM_NAME_LEN - 1)
		goto error_end;

	__test_fd = shm_open(name, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__test_fd > 0)
		goto error_end;

	__fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__fd < 0)
		goto error_end;

	if(ftruncate(__fd, size + sizeof(struct shm_ptr)) < 0)
		goto error_end;
	
	__ptr = malloc(sizeof(struct shm_ptr));
	if(!__ptr)
		goto error_end;

	memset(__ptr, 0, sizeof(struct shm_ptr));
	strncpy(__ptr->name, name, sizeof(__ptr->name));

	__ptr->addr = mmap(NULL, size + sizeof(struct shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, __fd, 0);
	if(__ptr->addr == MAP_FAILED)
		goto error_end;
	
	__ptr->size = size;

	if(write(__fd, __ptr, sizeof(struct shm_ptr)) != sizeof(struct shm_ptr))
		goto error_end;
	
	__ptr->addr += sizeof(struct shm_ptr);

	return __ptr;

error_end:
	perror("error occured.");
	if(__fd > 0)
		shm_unlink(name);

	if(__ptr)
		free(__ptr);

	return NULL; 
}

struct shm_ptr* shmem_open(const char* name)
{
	int __fd = 0;
	struct shm_ptr* __ptr = NULL;
	long page_size = sysconf(_SC_PAGESIZE);

	__fd = shm_open(name, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(__fd < 0)
		goto error_end;

	__ptr = malloc(sizeof(struct shm_ptr));
	if(!__ptr)
		goto error_end;

	if(read(__fd, __ptr, sizeof(struct shm_ptr)) != sizeof(struct shm_ptr))
		goto error_end;

	__ptr->addr -= sizeof(struct shm_ptr);

	__ptr->addr = mmap(__ptr->addr, __ptr->size + sizeof(struct shm_ptr), PROT_READ | PROT_WRITE, MAP_SHARED, __fd, 0);
	if(__ptr->addr == MAP_FAILED)
		goto error_end;

	__ptr->addr += sizeof(struct shm_ptr);

	return __ptr;

error_end:
	perror("error occured.");

	if(__fd > 0)
		shm_unlink(name);
	if(__ptr)
		free(__ptr);

	return NULL;
}

void shmem_close(struct shm_ptr* ptr)
{
	if(ptr == NULL)
		goto error_end;

	if(ptr->addr == NULL)
		goto error_end;

	if(munmap(ptr->addr - sizeof(struct shm_ptr), ptr->size + sizeof(struct shm_ptr)) < 0)
		perror("munmap error.");

	if(shm_unlink(ptr->name) < 0)
		perror("shm_unlink error.");

error_end:
	return;
}