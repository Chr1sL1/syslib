#ifndef __pgpool_h__
#define __pgpool_h__

struct pgpool
{
	void* addr;
	long size;
};

struct pgpool_config
{
	unsigned long maxpg_count;
};

struct pgpool* pgp_new(void* addr, long size, struct pgpool_config* cfg);
struct pgpool* pgp_load(void* addr, long size);

void pgp_del(struct pgpool* up);

void* pgp_alloc(struct pgpool* up, long size);
long pgp_free(struct pgpool* up, void* p);


void pgp_check(struct pgpool* up);

#endif

