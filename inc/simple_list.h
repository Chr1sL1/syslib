#ifndef __list_h__
#define __list_h__

struct lst_node
{
	struct lst_node* prev;
	struct lst_node* next;
};

struct simple_list
{
	int size;
	struct lst_node head;
	struct lst_node tail;
};

int lst_new(struct simple_list* lst);
int lst_clr(struct lst_node* node);


int lst_insert_before(struct simple_list* lst, struct lst_node* suc, struct lst_node* node);
int lst_insert_after(struct simple_list* lst, struct lst_node* prv, struct lst_node* node);
int lst_remove(struct simple_list* lst, struct lst_node* node);



#endif
