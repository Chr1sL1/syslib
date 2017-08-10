#ifndef __mm_zone_h__
#define __mm_zone_h__

enum MMZ_BLOCK_TYPE
{
	MMZ_INVALID = -1,

	MMZ_BEGIN,
	MMZ_TINY_BLOCK = MMZ_BEGIN,
	MMZ_BIG_BLOCK,
	MMZ_FIXED_BLOCK,

	MMZ_COUNT,
};

struct mm_zone
{
	long type;
	void* addr_begin;
	void* addr_end;
};

struct mm_zone_config
{
	union
	{
		struct
		{
			unsigned int min_block_order;
			unsigned int max_block_order;
		} mmp_cfg;

		struct
		{
			unsigned long maxpg_count;
		} pgp_cfg;

		struct
		{
			unsigned long obj_size;
		} slb_cfg;
	};
};

struct dlnode;
struct rbnode;

struct mm_zone* mmz_create(long type, void* addr_begin, void* addr_end, struct mm_zone_config* cfg);
struct mm_zone* mmz_load(void* addr);
void mmz_destroy(struct mm_zone* mmz);

void* mmz_alloc(struct mm_zone* mmz, unsigned long size);
long mmz_free(struct mm_zone* mmz, void* p);

struct dlnode* mmz_lstnode(struct mm_zone* mmz);
struct rbnode* mmz_rbnode(struct mm_zone* mmz);


#endif

