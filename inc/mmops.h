#ifndef __mm_ops_h__
#define __mm_ops_h__

struct mm_config
{
	unsigned long total_size;

	union
	{
		struct
		{
			unsigned int min_order;
			unsigned int max_order;
		};

		struct
		{
			unsigned int page_size;
			unsigned int maxpg_count;
		};
	};
};

typedef void* (*mm_ops_create)(void*, struct mm_config*);
typedef void* (*mm_ops_load)(void*);
typedef void (*mm_ops_destroy)(void*);

typedef void* (*mm_ops_alloc)(void*, unsigned long);
typedef long (*mm_ops_free)(void*, void*);


struct mm_ops
{
	mm_ops_create create_func;
	mm_ops_load load_func;
	mm_ops_destroy destroy_func;

	mm_ops_alloc alloc_func;
	mm_ops_free free_func;
};



#endif

