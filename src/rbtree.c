#include <stdio.h>
#include "rbtree.h"

#define is_black(node)	(!(node) || (node)->isblack)
#define is_red(node)	((node) && !(node)->isblack)
#define set_black(node)	((node) ? (node)->isblack = 1 : 0)
#define set_red(node)	((node) ? (node)->isblack = 0 : 0)

static void _cp_color(struct rbnode* to, struct rbnode* from)
{
	if(to)
		to->isblack = from ? from->isblack : 1;
}

void rb_fillnew(struct rbtree* t, struct rbnode* node)
{
	if(node)
	{
		node->key = 0;
		node->isblack = 0;
		node->lchild = NULL;
		node->rchild = NULL;
		node->parent = NULL;
	}
}

void _left_rotate(struct rbnode* node)
{
	struct rbnode* rc = NULL;
	if(node == NULL) goto error_ret;

	rc = node->rchild;
	if(rc == NULL) goto error_ret;

	rc->parent = node->parent;

	if(node->parent)
	{
		if(node == node->parent->lchild)
			node->parent->lchild = rc;
		else
			node->parent->rchild = rc;
	}

	node->parent = rc;

	if(rc->lchild)
		rc->lchild->parent = node;

	node->rchild = rc->lchild;

	rc->lchild = node;

error_ret:
	return;
}

void _right_rotate(struct rbnode* node)
{
	struct rbnode* lc = NULL;
	if(node == NULL) goto error_ret;

	lc = node->lchild;
	if(lc == NULL) goto error_ret;

	lc->parent = node->parent;

	if(node->parent)
	{
		if(node == node->parent->lchild)
			node->parent->lchild = lc;
		else
			node->parent->rchild = lc;
	}

	node->parent = lc;

	if(lc->rchild)
		lc->rchild->parent = node;

	node->lchild = lc->rchild;

	lc->rchild = node;


error_ret:
	return;
}


static void _fix_insert(struct rbtree* t, struct rbnode* node)
{
	struct rbnode* p = NULL, *g = NULL, *u = NULL;
	if(!node) goto error_ret;

	p = node->parent;
	if(!p) goto error_ret;

	while(is_red(node->parent))
	{
		p = node->parent;
		g = p->parent;

		if(p == g->lchild)
		{
			u = g->rchild;
			if(is_red(u))
			{
				set_black(p);
				set_black(u);
				set_red(g);
				node = g;
			}
			else
			{
				if(node == p->rchild)		// zigzag
				{
					_left_rotate(node->parent);
				}
				node = node->parent;

				set_black(node);
				set_red(node->parent);
				_right_rotate(node->parent);

				break;
			}
		}
		else
		{
			u = g->lchild;
			if(is_red(u))
			{
				set_black(p);
				set_black(u);
				set_red(g);
				node = g;
			}
			else
			{
				if(node == p->lchild)		// zigzag
				{
					_right_rotate(node->parent);
				}
				node = node->parent;

				set_black(node);
				set_red(node->parent);
				_left_rotate(node->parent);

				break;
			}
		}

	}

	while(node)
	{
		if(!node->parent)
			break;

		node = node->parent;
	}
	t->root = node;
	set_black(t->root);

error_ret:
	return;
}

int rb_insert(struct rbtree* t, struct rbnode* node)
{
	struct rbnode* hot = NULL;

	if(!node) goto error_ret;
	if(rb_search(t, node->key, &hot) != NULL) goto error_ret;

	node->isblack = 0;
	node->lchild = NULL;
	node->rchild = NULL;
	node->parent = hot;

	if(hot != NULL)
	{
		if(node->key < hot->key)
			hot->lchild = node;
		else
			hot->rchild = node;

		node->parent = hot;

		_fix_insert(t, node);
	}
	else
		t->root = node;

	t->root->isblack = 1;

	++t->size;
	return 1;
error_ret:
	return 0;
}

struct rbnode* rb_search(struct rbtree* t, int key, struct rbnode** hot)
{
	struct rbnode* p = t->root;
	*hot = t->root;

	if(!t->root) goto error_ret;

	while(p && p->key != key)
	{
		*hot = p;
		if(key < p->key)
			p = p->lchild;
		else if(key > p->key)
			p = p->rchild;
	}

	return p;
error_ret:
	return NULL;
}

static void _link(struct rbnode* p, struct rbnode* lc, struct rbnode* rc)
{
	if(!p) goto error_ret;
	if(p == lc || p == rc) goto error_ret;

	p->lchild = lc;
	p->rchild = rc;

	lc ? (lc->parent = p) : 0;
	rc ? (rc->parent = p) : 0;

error_ret:
	return;
}

static void _transplant(struct rbtree* t, struct rbnode* rm, struct rbnode* sc)
{
	if(!t->root) goto error_ret;
	if(!rm) goto error_ret;

	if(!rm->parent)	 // root
	{
		_link(sc, rm->lchild, rm->rchild);
		t->root = sc;
		if(sc) sc->parent = NULL;
	}
	else if(rm == rm->parent->lchild)
		_link(rm->parent, sc, rm->parent->rchild);
	else if(rm == rm->parent->rchild)
		_link(rm->parent, rm->parent->lchild, sc);


	rm->parent = NULL;
	rm->lchild = NULL;
	rm->rchild = NULL;

error_ret:
	return;
}

static struct rbnode* _minnode(struct rbnode* subroot)
{
	struct rbnode* p = subroot;
	if(!subroot) goto error_ret;

	while(subroot)
	{
		p = subroot;
		subroot = subroot->lchild;
	}

	return p;
error_ret:
	return NULL;
}

// to be fixed.
static void _fix_remove(struct rbtree* t, struct rbnode* x)
{
	struct rbnode* w = NULL;
	if(!x) goto error_ret;

	while(x != t->root && is_black(x))
	{
		if(x == x->parent->lchild)
		{
			w = x->parent->rchild;
			if(is_red(w))
			{
				set_black(w);
				set_red(x->parent);
				_left_rotate(x->parent);
			}
			w = x->parent->rchild;

			if(is_black(w->lchild) && is_black(w->rchild))
			{
				set_red(w);
				x = x->parent;
			}
			else if(is_black(w->rchild))
			{
				set_black(w->lchild);
				set_red(w);
				_right_rotate(w);
				w = x->parent->rchild;
			}
			
			_cp_color(w, x->parent);
			set_black(x->parent);
			set_black(w->rchild);
			_left_rotate(x->parent);
			x = t->root;
		}
		else
		{
			w = x->parent->lchild;
			if(is_red(w))
			{
				set_black(w);
				set_red(x->parent);
				_right_rotate(x->parent);
			}
			w = x->parent->lchild;

			if(is_black(w->rchild) && is_black(w->lchild))
			{
				set_red(w);
				x = x->parent;
			}
			else if(is_black(w->lchild))
			{
				set_black(w->rchild);
				set_red(w);
				_left_rotate(w);
				w = x->parent->lchild;
			}
			
			_cp_color(w, x->parent);
			set_black(x->parent);
			set_black(w->lchild);
			_right_rotate(x->parent);
			x = t->root;
		}
	}

	set_black(x);
error_ret:
	return;
}

struct rbnode* rb_remove(struct rbtree* t, int key)
{
	struct rbnode* x = NULL;
	struct rbnode* z = t->root;
	struct rbnode* y = NULL;
	int node_is_black = 0;

	if(!t->root) goto error_ret;
	if(t->size <= 0) return NULL;

	z = rb_search(t, key, &x);
	if(!z) goto error_ret;

	y = z;
	node_is_black = is_black(y);

	if(!z->lchild)
	{
		x = z->rchild;
		_transplant(t, z, x);
	}
	else if(!z->rchild)
	{
		x = z->lchild;
		_transplant(t, z, x);
	}

	if(z->lchild && z->rchild)
	{
		y = _minnode(z->rchild);
		node_is_black = is_black(y);

		x = y->rchild;
		
		_transplant(t, y, x);
		_transplant(t, z, y);
		_cp_color(y, z);
	}

	if(node_is_black)
		_fix_remove(t, x);

//	if(z->parent)
//	{
//		if(z == z->parent->lchild)
//			z->parent->lchild = NULL;
//		else if(z == z->parent->rchild)
//			z->parent->rchild = NULL;
//	}

	z->parent = NULL;
	z->lchild = NULL;
	z->rchild = NULL;
	
	--t->size;

	if(t->size <= 0)
		t->root = NULL;

	return z;
error_ret:
	return NULL;
}

void pre_order(struct rbnode* node)
{
	if(!node)
		return;

	printf("%d(%d),", node->key, node->isblack);
	pre_order(node->lchild);
	pre_order(node->rchild);
}

void in_order(struct rbnode* node)
{
	if(!node)
		return;

	in_order(node->lchild);
	printf("%d(%d),", node->key, node->isblack);
	in_order(node->rchild);
}

void post_order(struct rbnode* node)
{
	if(!node)
		return;

	pre_order(node->lchild);
	pre_order(node->rchild);
	printf("%d(%d),", node->key, node->isblack);
}
