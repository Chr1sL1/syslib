#include "mmzone.h"
#include "dlist.h"
#include "mmspace.h"

#include <stdlib.h>

struct _free_list_node
{
	struct dlnode _fln;
	void* p;
};

struct _mmzone_impl
{
	struct mmzone _the_zone;
	struct dlist _free_list;
};

static inline struct _mmzone_impl* _conv_impl(struct mmzone* mmz)
{
	return (struct _mmzone_impl*)((unsigned long)mmz - (unsigned long)(&((struct _mmzone_impl*)(0))->_the_zone));
}

static inline struct _free_list_node* _conv_fln(struct dlnode* dln)
{
	return (struct _free_list_node*)((unsigned long)dln- (unsigned long)(&((struct _free_list_node*)(0))->_fln));
}

struct mmzone* mmz_create(unsigned long obj_size)
{
	struct _mmzone_impl* mzi;
	if(obj_size == 0) goto error_ret;

	mzi = malloc(sizeof(struct _mmzone_impl));
	if(!mzi) goto error_ret;

	mzi->_the_zone.obj_size = obj_size;
	mzi->_the_zone.current_free_count = 0;

	lst_new(&mzi->_free_list);

	return &mzi->_the_zone;
error_ret:
	return 0;
}

long mmz_destroy(struct mmzone* mmz)
{
	struct dlnode* dln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	//return all freenode and data to mmspace.
	//

	dln = &mzi->_free_list.head;
	while(dln != &mzi->_free_list.tail)
	{
		struct _free_list_node* fln = _conv_fln(dln);
		mm_free(fln->p);
		mm_free(fln);

		dln = dln->next;
	}

	//

	free(mzi);

	return 0;
error_ret:
	return -1;
}

void* mmz_alloc(struct mmzone* mmz)
{
	void* p;
	struct dlnode* dln;
	struct _free_list_node* fln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	if(!lst_empty(&mzi->_free_list))
	{
		dln = lst_pop_front(&mzi->_free_list);
		fln = _conv_fln(dln);
		p = fln->p;
		mm_free(fln);

		--mzi->_the_zone.current_free_count;
	}
	else
		p = mm_alloc(mzi->_the_zone.obj_size);

	return p;
error_ret:
	return 0;
}

long mmz_free(struct mmzone* mmz, void* p)
{
	long rslt;
	struct _free_list_node* fln;
	struct _mmzone_impl* mzi = _conv_impl(mmz);
	if(!mzi) goto error_ret;

	fln = mm_alloc(sizeof(struct _free_list_node));
	if(!fln) goto error_ret;

	fln->p = p;
	rslt = lst_push_front(&mzi->_free_list, &fln->_fln);
	if(!rslt) goto error_ret;

	++mzi->_the_zone.current_free_count;

	return 0;
error_ret:
	return -1;
}

