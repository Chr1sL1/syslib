#ifndef __rbtree_h__
#define __rbtree_h__

struct rbnode
{
	unsigned long key;
	struct rbnode* lchild;
	struct rbnode* rchild;
	union
	{
		struct rbnode* p;
		unsigned isblack : 2;
	};
};

struct rbtree
{
	int size;
	int depth;
	struct rbnode* root;
};

typedef void (*order_function)(struct rbnode* node);

void rb_fillnew(struct rbnode* node);
struct rbnode* rb_parent(struct rbnode* node);
struct rbnode* rb_sibling(struct rbnode* node);
struct rbnode* rb_succ(struct rbnode* node);

long rb_insert(struct rbtree* t, struct rbnode* node);
struct rbnode* rb_search(struct rbtree* t, unsigned long key, struct rbnode** hot);
struct rbnode* rb_remove(struct rbtree* t, unsigned long key);
void rb_remove_node(struct rbtree* t, struct rbnode* x);

void rb_traverse(struct rbtree* t, order_function f);

//debug issue
void pre_order(struct rbnode* node, order_function f);
void in_order(struct rbnode* node, order_function f);
void post_order(struct rbnode* node, order_function f);

#endif
