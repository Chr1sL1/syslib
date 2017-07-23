#include "uma.h"
#include "dlist.h"
#include <stdlib.h>

#define UMA_LABEL (0xeded00122100dedeUL)
#define UMA_OBJ_LABEL (0xabcd7890)
#define ALIGN (8)

#define UFB_USED (1)

#pragma pack(1)

struct _uma_header
{
	unsigned long _chunk_label;
	unsigned long _chunk_size;
	unsigned long _obj_size;
};

#pragma pack()

struct _uma_obj_header
{
	unsigned int _obj_label;
	unsigned int _obj_flag;
};

struct _uma_node
{
	struct dlnode _fln;
	struct _uma_obj_header* _obj;
};

struct _uma_impl
{
	struct uma _the_uma;
	struct dlist _free_list;

	void* _chunk_addr;
	void* _chunk_end;

	unsigned long _obj_size;
	unsigned long _obj_count;

	struct _uma_node* _node_pool;
};

//static inline void _set_payload(struct _uma_node* node, void* payload)
//{
//	node->_payload_addr = (void*)((unsigned long)payload | node->using);
//}
//
static inline void* _get_payload(struct _uma_obj_header* uoh)
{
	return (void*)(uoh + 1);
}

static inline void _set_flag(struct _uma_obj_header* uoh, unsigned int flag)
{
	uoh->_obj_flag |= flag;
}

static inline void _clear_flag(struct _uma_obj_header* uoh, unsigned int flag)
{
	uoh->_obj_flag &= ~flag;
}

static inline struct _uma_impl* _conv_impl(struct uma* mm)
{
	return (struct _uma_impl*)((void*)mm - (unsigned long)(&((struct _uma_impl*)(0))->_the_uma));
}

static inline struct _uma_node* _conv_fln(struct dlnode* fln)
{
	return (struct _uma_node*)((void*)fln - (unsigned long)(&((struct _uma_node*)(0))->_fln));
}

static inline struct _uma_node* _get_node_from_obj(struct _uma_impl* umi, void* uoh)
{
	unsigned long idx;

	if(uoh < umi->_chunk_addr || uoh > umi->_chunk_end)
		goto error_ret;

	idx = (uoh - umi->_chunk_addr) / umi->_obj_size;
	if(idx >= umi->_obj_count)
		goto error_ret;

	return &umi->_node_pool[idx];
error_ret:
	return 0;
}

static inline struct _uma_node* _fetch_fln(struct _uma_impl* umi)
{
	struct dlnode* dln = lst_pop_front(&umi->_free_list);
	if(!dln) goto error_ret;

	lst_clr(dln);

	return _conv_fln(dln);
error_ret:
	return 0;
}

static inline void _return_fln(struct _uma_impl* umi, struct _uma_node* un)
{
	lst_clr(&un->_fln);
	lst_push_front(&umi->_free_list, &un->_fln);
}

struct uma* uma_create(void* addr, unsigned long size, unsigned long obj_size)
{
	struct _uma_impl* umi;
	struct _uma_header* hd;
	unsigned long chunk_size;

	if(!addr) goto error_ret;

	umi = malloc(sizeof(struct _uma_header));
	if(!umi) goto error_ret;

	umi->_the_uma.addr = addr;
	umi->_chunk_addr = (void*)((unsigned long)(addr + sizeof(struct _uma_header) + (ALIGN - 1)) & ~(ALIGN - 1));

	hd = (struct _uma_header*)(umi->_chunk_addr - sizeof(struct _uma_header));

	umi->_obj_size = (obj_size + (ALIGN - 1)) & ~(ALIGN - 1) + sizeof(struct _uma_obj_header);
	chunk_size = size - (umi->_the_uma.addr - addr);
	umi->_obj_count = chunk_size / umi->_obj_size;

	umi->_chunk_end = umi->_chunk_addr + umi->_obj_size * (umi->_obj_count - 1);

	hd->_chunk_label = UMA_LABEL;
	hd->_chunk_size = chunk_size;
	hd->_obj_size = umi->_obj_size;

	umi->_node_pool = malloc(sizeof(struct _uma_node) * umi->_obj_count);
	if(!umi) goto error_ret;

	lst_new(&umi->_free_list);

	for(unsigned long i = 0; i < umi->_obj_count; ++i)
	{
		struct _uma_obj_header* uoh = (struct _uma_obj_header*)(umi->_chunk_addr + i * umi->_obj_size);
		uoh->_obj_label = UMA_OBJ_LABEL;
		uoh->_obj_flag = 0;

		umi->_node_pool[i]._obj = uoh;

		lst_clr(&umi->_node_pool[i]._fln);
		lst_push_back(&umi->_free_list, &umi->_node_pool[i]._fln);
	}

	return &umi->_the_uma;
error_ret:
	if(umi)
		uma_destroy(&umi->_the_uma);
	return 0;
}

struct uma* uma_load(void* addr)
{
	struct _uma_impl* umi;
	struct _uma_header* hd;

	if(!addr) goto error_ret;

	umi = malloc(sizeof(struct _uma_header));
	if(!umi) goto error_ret;

	umi->_the_uma.addr = addr;
	umi->_chunk_addr = (void*)((unsigned long)(addr + sizeof(struct _uma_header) + (ALIGN - 1)) & ~(ALIGN - 1));

	hd = (struct _uma_header*)(umi->_chunk_addr - sizeof(struct _uma_header));

	if(hd->_chunk_label != UMA_LABEL) goto error_ret;
	umi->_obj_size = hd->_obj_size;
	umi->_obj_count = hd->_chunk_size / umi->_obj_size;
	umi->_chunk_end = umi->_chunk_addr + umi->_obj_size * (umi->_obj_count - 1);

	umi->_node_pool = malloc(sizeof(struct _uma_node) * umi->_obj_count);
	if(!umi) goto error_ret;

	lst_new(&umi->_free_list);

	for(unsigned long i = 0; i < umi->_obj_count; ++i)
	{
		struct _uma_obj_header* uoh = (struct _uma_obj_header*)(umi->_chunk_addr + i * umi->_obj_size);
		if(uoh->_obj_label != UMA_OBJ_LABEL)
			goto error_ret;

		if(uoh->_obj_flag & UFB_USED == 0)
		{
			umi->_node_pool[i]._obj = uoh;
			lst_clr(&umi->_node_pool[i]._fln);
			lst_push_back(&umi->_free_list, &umi->_node_pool[i]._fln);
		}
	}

error_ret:
	if(umi)
		uma_destroy(&umi->_the_uma);
	return 0;
}

void uma_destroy(struct uma* mm)
{
	struct _uma_impl* umi = _conv_impl(mm);
	if(umi)
	{
		if(umi->_node_pool)
			free(umi->_node_pool);

		free(umi);
	}
}

void* uma_alloc(struct uma* mm)
{
	struct _uma_node* un;
	struct _uma_obj_header* uoh;
	struct _uma_impl* umi = _conv_impl(mm);
	if(!umi) goto error_ret;

	un = _fetch_fln(umi);
	if(!un) goto error_ret;

	uoh = un->_obj;
	if(!uoh || uoh->_obj_label != UMA_OBJ_LABEL || (uoh->_obj_flag & UFB_USED) != 0)
		goto error_ret;

	_set_flag(uoh, UFB_USED);

	return _get_payload(uoh);
error_ret:
	return 0;
}

long uma_free(struct uma* mm, void* p)
{
	struct _uma_node* un;
	struct _uma_obj_header* uoh;
	struct _uma_impl* umi = _conv_impl(mm);
	if(!umi) goto error_ret;

	uoh = (struct _uma_obj_header*)(p - sizeof(struct _uma_obj_header));

	if(uoh->_obj_label != UMA_OBJ_LABEL || (uoh->_obj_flag & UFB_USED) == 0)
		goto error_ret;

	un = _get_node_from_obj(umi, uoh);
	if(!un || un->_obj != uoh)
		goto error_ret;

	_clear_flag(uoh, UFB_USED);

	_return_fln(umi, un);

	return 0;
error_ret:
	return -1;
}

