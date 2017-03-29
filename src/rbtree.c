#include <stdio.h>
#include "rbtree.h"

#define is_black(node)	(!(node) || (node)->isblack)
#define is_red(node)	((node) && !(node)->isblack)
#define set_black(node)	((node) ? (node)->isblack = 1 : 0)
#define set_red(node)	((node) ? (node)->isblack = 0 : 0)
#define l_child(node)	((node) ? (node)->lchild : NULL)
#define r_child(node)	((node) ? (node)->rchild : NULL)

static void _cp_color(struct rbnode* to, struct rbnode* from)
{
	if(to)
		to->isblack = from ? from->isblack : 1;
}

static void _swap_value(int* v1, int* v2)
{
	int tmp = *v1;
	*v1 = *v2;
	*v2 = tmp;
}

void rb_fillnew(struct rbnode* node)
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

static void _left_rotate(struct rbnode* node)
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

static void _right_rotate(struct rbnode* node)
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


static void _fix_rr(struct rbtree* t, struct rbnode* node)
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
				else
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
				else
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

		_fix_rr(t, node);
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
	struct rbnode* h = NULL;

	h = t->root;

	if(!t->root) goto error_ret;

	while(p && p->key != key)
	{
		h = p;
		if(key < p->key)
			p = p->lchild;
		else if(key > p->key)
			p = p->rchild;
	}

	if(hot) *hot = h;

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


static struct rbnode* _succ(struct rbnode* subroot)
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

static struct rbnode* _do_remove(struct rbtree* t, struct rbnode* x, struct rbnode** r)
{
	struct rbnode* w = x;

	if(!t->root) goto error_ret;
	if(!x) goto error_ret;

	if(!w->rchild)
	{
		*r = w->lchild;
	}
	else if(!w->lchild)
	{
		*r = w->rchild;
	}
	else
	{
		w = _succ(x->rchild);
		*r = w->rchild;
	}

	_swap_value(&x->key, &w->key);

	_transplant(t, w, *r);

	return w;
error_ret:
	return NULL;
}

static void _fix_bb(struct rbtree* tree, struct rbnode* x)
{
	struct rbnode* p = NULL;
	struct rbnode* s = NULL;
	struct rbnode* t = NULL;

	if(is_red(x)) goto error_ret;

	while(x)
	{
		p = x->parent;
		if(!p)
			break;

		if(x == p->rchild)
		{
			s = p->lchild;
			if(is_black(s))
			{
				if(is_red(l_child(s)) || is_red(r_child(s)))
				{
					//bb1:
					if(is_red(s->lchild)) t = l_child(s);
					else if(is_red(s->rchild)) t = r_child(s);

					_right_rotate(p);
					p->isblack ? set_black(s) : set_red(s);
					set_black(p);
					set_black(t);
					break;
				}
				else
				{
					if(is_red(p))
					{
						//bb2r:
						set_black(p);
						set_red(s);
						break;
					}
					else
					{
						//bb2b:
						set_red(s);
						x = p;
						continue;
					}
				}
			}
			else //bb3:
			{
				// p must be black;
				_right_rotate(p);
				set_black(s);
				set_red(p);
				// then turns in to bb1 / bb2r in the next loop;
			}
		}
		else
		{
			s = p->rchild;
			if(is_black(s))
			{
				if(is_red(r_child(s)) || is_red(l_child(s)))
				{
					//bb1:
					if(is_red(s->rchild)) t = r_child(s);
					else if(is_red(s->lchild)) t = l_child(s);

					_left_rotate(p);
					p->isblack ? set_black(s) : set_red(s);
					set_black(p);
					set_black(t);
					break;
				}
				else
				{
					if(is_red(p))
					{
						//bb2r:
						set_black(p);
						set_red(s);
						break;
					}
					else
					{
						//bb2b:
						set_red(s);
						x = p;
						continue;
					}
				}
			}
			else //bb3:
			{
				// p must be black;
				_left_rotate(p);
				set_black(s);
				set_red(p);
				// then turns in to bb1 / bb2r in the next loop;
			}
		}
	}

	while(x)
	{
		if(!x->parent)
			break;

		x = x->parent;

		tree->root = x;
	}

	set_black(tree->root);

error_ret:
	return;
}

struct rbnode* rb_remove(struct rbtree* t, int key)
{
	struct rbnode* r = NULL;
	struct rbnode* x = t->root;
	struct rbnode* tmp = NULL;
	int isredr = 0;
	int isredx = 0;

	if(!t->root) goto error_ret;
	if(t->size <= 0) return NULL;

	x = rb_search(t, key, &r);
	if(!x) goto error_ret;

	isredx = is_red(x);
	x = _do_remove(t, x, &r);
	isredr = is_red(r);

	if(isredx || isredr)
		set_black(r);
	else
		_fix_bb(t, r);

	set_black(t->root);

	x->parent = NULL;
	x->lchild = NULL;
	x->rchild = NULL;
	
	--t->size;
	if(t->size <= 0)
		t->root = NULL;

	return x;
error_ret:
	return NULL;
}

void pre_order(struct rbnode* node, order_function f)
{
	if(!node || !f)
		return;

	f(node);
	pre_order(node->lchild, f);
	pre_order(node->rchild, f);
}

void in_order(struct rbnode* node, order_function f)
{
	if(!node || !f)
		return;

	in_order(node->lchild, f);
	f(node);
	in_order(node->rchild, f);
}

void post_order(struct rbnode* node, order_function f)
{
	if(!node || !f)
		return;

	post_order(node->lchild, f);
	post_order(node->rchild, f);
	f(node);
}
