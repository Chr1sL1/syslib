#ifndef __pgpool_h__
#define __pgpool_h__

struct pgpool
{
	void* addr;
	long size;
};

struct pgpool_config
{
	unsigned long max_pg;
};

struct pgpool* pgp_new(void* addr, long size, struct pgpool_config* cfg);
struct pgpool* pgp_load(void* addr, long size);

void pgp_del(struct pgpool* up);

void* pgp_alloc(long size);
long pgp_free(void* p);

#endif

