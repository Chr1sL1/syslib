#ifndef __rbtree_h__
#define __rbtree_h__

struct rbnode
{
	int key;
	int isblack;
	struct rbnode* lchild;
	struct rbnode* rchild;
	struct rbnode* parent;
};

struct rbtree
{
	int size;
	struct rbnode* root;
};

typedef void (*order_function)(struct rbnode* node);

void rb_fillnew(struct rbtree* t, struct rbnode* node);
int rb_insert(struct rbtree* t, struct rbnode* node);
struct rbnode* rb_search(struct rbtree* t, int key, struct rbnode** hot);
struct rbnode* rb_remove(struct rbtree* t, int key);


//debug issue
void pre_order(struct rbnode* node, order_function f);
void in_order(struct rbnode* node, order_function f);
void post_order(struct rbnode* node, order_function f);

#endif
