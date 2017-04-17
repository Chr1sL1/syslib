#include <stdio.h>
#include "dlist.h"

int lst_new(struct dlist* lst)
{
	if(!lst) goto error_ret; 

	lst->head.prev = NULL;
	lst->head.next = &lst->tail;

	lst->tail.prev = &lst->head;
	lst->tail.next = NULL;

	lst->size = 0;

	return 1;
error_ret:
	return 0;
}

int lst_clr(struct dlnode* node)
{
	if(!node) goto error_ret;

	node->prev = NULL;
	node->next = NULL;

	return 1;
error_ret:
	return 0;
}


int lst_insert_before(struct dlist* lst, struct dlnode* suc, struct dlnode* node)
{
	if(!lst || !suc || !node) goto error_ret;
	if(suc == &lst->head) goto error_ret;

	node->prev = suc->prev;
	node->next = suc;

	suc->prev->next = node;
	suc->prev = node;

	++lst->size;

	printf("head: %p, tail: %p.", &lst->head, &lst->tail);
	printf("node: %p, prev: %p, next: %p.\n", node, node->prev, node->next);

	return 1;
error_ret:
	return 0;
}

int lst_insert_after(struct dlist* lst, struct dlnode* prv, struct dlnode* node)
{
	if(!lst || !prv || !node) goto error_ret;
	if(prv == &lst->tail) goto error_ret;

	node->next = prv->next;
	node->prev = prv;

	prv->next->prev = node;
	prv->next = node;

	++lst->size;

	return 1;
error_ret:
	return 0;
}

int lst_remove(struct dlist* lst, struct dlnode* node)
{
	if(!lst || !node) goto error_ret;
	if(lst->tail.prev == &lst->head || lst->head.next == &lst->tail) goto error_ret;
	if(node == &lst->head || node == &lst->tail) goto error_ret;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	node->prev = NULL;
	node->next = NULL;
	
	--lst->size;

	return 1;
error_ret:
	return 0;
}

int lst_push_back(struct dlist* lst, struct dlnode* node)
{
	return lst_insert_before(lst, &lst->tail, node);
error_ret:
	return 0;
}

int lst_push_front(struct dlist* lst, struct dlnode* node)
{
	return lst_insert_after(lst, &lst->head, node);
error_ret:
	return 0;
}

struct dlnode* lst_pop_back(struct dlist* lst)
{
	if(!lst) goto error_ret;

	struct dlnode* node = lst->tail.prev;
	if(!lst_remove(lst, node)) goto error_ret;
	return node;
error_ret:
	return NULL;
}

struct dlnode* lst_pop_front(struct dlist* lst)
{
	if(!lst) goto error_ret;

	struct dlnode* node = lst->head.next;
	if(!lst_remove(lst, node)) goto error_ret;
	return node;
error_ret:
	return NULL;
}


