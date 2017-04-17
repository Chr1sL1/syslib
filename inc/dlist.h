#ifndef __dlist_h__
#define __dlist_h__

struct dlnode
{
	struct dlnode* prev;
	struct dlnode* next;
};

struct dlist
{
	int size;
	struct dlnode head;
	struct dlnode tail;
};

int lst_new(struct dlist* lst);
int lst_clr(struct dlnode* node);


int lst_insert_before(struct dlist* lst, struct dlnode* suc, struct dlnode* node);
int lst_insert_after(struct dlist* lst, struct dlnode* prv, struct dlnode* node);

int lst_remove(struct dlist* lst, struct dlnode* node);

int lst_push_back(struct dlist* lst, struct dlnode* node);
int lst_push_front(struct dlist* lst, struct dlnode* node);

struct dlnode* lst_pop_back(struct dlist* lst);
struct dlnode* lst_pop_front(struct dlist* lst);

#endif
