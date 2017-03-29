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

