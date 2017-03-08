#include <stdio.h>
#include "shmem.h"

const char* share_memory_name = "test_shm_16x";

int main(void)
{
	struct shm_host_ptr* __ptr = shmem_new(share_memory_name, 1024 * 1024 * 1024);
	struct shm_client_ptr* __read_ptr = NULL;

	if(!__ptr)
		return -1;

	sprintf((char*)__ptr->_the_addr._addr, "hello world.");

	__read_ptr = shmem_open(share_memory_name);

	if(!__read_ptr)
		return -1;

	printf("content: %s\n", (char*)__read_ptr->_the_addr._addr);

	shmem_close(__read_ptr);
	shmem_destroy(__ptr);

	return 0;
}
