#ifndef __mm_zone_h__
#define __mm_zone_h__


struct mmzone
{
	unsigned long obj_size;
	unsigned long current_free_count;
};

struct mmzone* mmz_create(unsigned long obj_size);
long mmz_destroy(struct mmzone* mmz);

void* mmz_alloc(struct mmzone* mmz);
long mmz_free(struct mmzone* mmz, void* p);

#endif
