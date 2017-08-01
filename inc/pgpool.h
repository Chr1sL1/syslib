#ifndef __pgpool_h__
#define __pgpool_h__

struct pgpool
{
	void* addr_begin;
	void* addr_end;
};

struct pgpool* pgp_create(void* addr, unsigned long size, unsigned long maxpg_count);
struct pgpool* pgp_load(void* addr);
void pgp_destroy(struct pgpool* up);

void* pgp_alloc(struct pgpool* up, unsigned long size);
long pgp_free(struct pgpool* up, void* p);


long pgp_check(struct pgpool* up);

#endif

