#include <stdio.h>
#include "simple_list.h"

int lst_new(struct simple_list* lst)
{
	if(!lst) goto error_ret; 

	lst->head.prev = NULL;
	lst->head.next = &lst->tail;

	lst->tail.prev = &lst->head;
	lst->tail.next = NULL;

	lst->size = 0;

	return 0;
error_ret:
	return -1;
}

int lst_clr(struct lst_node* node)
{
	if(!node) goto error_ret;

	node->prev = NULL;
	node->next = NULL;

	return 0;
error_ret:
	return -1;
}


int lst_insert_before(struct simple_list* lst, struct lst_node* suc, struct lst_node* node)
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

int lst_insert_after(struct simple_list* lst, struct lst_node* prv, struct lst_node* node)
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

int lst_remove(struct simple_list* lst, struct lst_node* node)
{
	if(!lst || !node) goto error_ret;
	if(node == &lst->head || node == &lst->tail) goto error_ret;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	node->prev = NULL;
	node->next = NULL;
	
	--lst->size;

	return 0;
error_ret:
	return -1;
}

