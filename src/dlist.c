#include "dlist.h"

long lst_new(struct dlist* lst)
{
	if(!lst) goto error_ret; 

	lst->head.prev = 0;
	lst->head.next = &lst->tail;

	lst->tail.prev = &lst->head;
	lst->tail.next = 0;

	lst->size = 0;

	return 0;
error_ret:
	return -1;
}

long lst_clr(struct dlnode* node)
{
	if(!node) goto error_ret;

	node->prev = 0;
	node->next = 0;

	return 0;
error_ret:
	return -1;
}


long lst_insert_before(struct dlist* lst, struct dlnode* suc, struct dlnode* node)
{
	if(!lst || !suc || !node) goto error_ret;
	if(suc == &lst->head) goto error_ret;

	node->prev = suc->prev;
	node->next = suc;

	suc->prev->next = node;
	suc->prev = node;

	++lst->size;

	return 0;
error_ret:
	return -1;
}

long lst_insert_after(struct dlist* lst, struct dlnode* prv, struct dlnode* node)
{
	if(!lst || !prv || !node) goto error_ret;
	if(prv == &lst->tail) goto error_ret;

	node->next = prv->next;
	node->prev = prv;

	prv->next->prev = node;
	prv->next = node;

	++lst->size;

	return 0;
error_ret:
	return -1;
}

long lst_remove(struct dlist* lst, struct dlnode* node)
{
	if(!lst || !node) goto error_ret;
	if(lst->tail.prev == &lst->head || lst->head.next == &lst->tail) goto error_ret;
	if(node == &lst->head || node == &lst->tail) goto error_ret;
	if(lst->size <= 0) goto error_ret;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	node->prev = 0;
	node->next = 0;
	
	--lst->size;

	return 0;
error_ret:
	return -1;
}

long lst_push_back(struct dlist* lst, struct dlnode* node)
{
	return lst_insert_before(lst, &lst->tail, node);
error_ret:
	return -1;
}

long lst_push_front(struct dlist* lst, struct dlnode* node)
{
	return lst_insert_after(lst, &lst->head, node);
error_ret:
	return -1;
}

struct dlnode* lst_pop_back(struct dlist* lst)
{
	if(!lst) goto error_ret;

	struct dlnode* node = lst->tail.prev;
	if(lst_remove(lst, node) < 0) goto error_ret;
	return node;
error_ret:
	return 0;
}

struct dlnode* lst_pop_front(struct dlist* lst)
{
	if(!lst) goto error_ret;

	struct dlnode* node = lst->head.next;
	if(lst_remove(lst, node) < 0) goto error_ret;
	return node;
error_ret:
	return 0;
}

struct dlnode* lst_first(struct dlist* lst)
{
	if(!lst || lst->size <= 0) goto error_ret;
	return lst->head.next;
error_ret:
	return 0;
}

struct dlnode* lst_last(struct dlist* lst)
{
	if(!lst || lst->size <= 0) goto error_ret;
	return lst->tail.prev;
error_ret:
	return 0;

}

long lst_check(struct dlist* lst)
{
	struct dlnode* node = lst->head.next;

	while(node != &lst->tail)
	{
		struct dlnode* suc = node->next;

		if(suc->prev != node)
			goto error_ret;


		node = suc;
	}

	return 0;
error_ret:
	return -1;
}


