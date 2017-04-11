#ifndef __rbtree_h__
#define __rbtree_h__


#pragma comment(push, 1)
struct rbnode
{
	int key;
	struct rbnode* lchild;
	struct rbnode* rchild;
	union
	{
		struct rbnode* p;
		unsigned isblack : 2;

//		struct
//		{
//			unsigned long parent : 63;
//			unsigned isblack : 1;
//		};
	};
};
#pragma comment(pop)

struct rbtree
{
	int size;
	int depth;
	struct rbnode* root;
};

typedef void (*order_function)(struct rbnode* node);

void rb_fillnew(struct rbnode* node);
int rb_insert(struct rbtree* t, struct rbnode* node);
struct rbnode* rb_search(struct rbtree* t, int key, struct rbnode** hot);
struct rbnode* rb_remove(struct rbtree* t, int key);

void rb_traverse(struct rbtree* t, order_function f);


//debug issue
void pre_order(struct rbnode* node, order_function f);
void in_order(struct rbnode* node, order_function f);
void post_order(struct rbnode* node, order_function f);

#endif
