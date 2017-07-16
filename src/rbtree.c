#include "rbtree.h"
#include "dlist.h"
#include "common.h"

#define is_black(node)	(!(node) || (node)->isblack)
#define is_red(node)	((node) && !(node)->isblack)
#define set_black(node)	((node) ? (node)->isblack = 1 : 0)
#define set_red(node)	((node) ? (node)->isblack = 0 : 0)
#define l_child(node)	((node) ? (node)->lchild : 0)
#define r_child(node)	((node) ? (node)->rchild : 0)

#define MAX_TRAVERSE_QUEUE_LEN (1024)

static inline void _cp_color(struct rbnode* to, struct rbnode* from)
{
	if(to)
		to->isblack = from ? from->isblack : 1;
}

static inline void _swap_value(long* v1, long* v2)
{
	int tmp = *v1;
	*v1 = *v2;
	*v2 = tmp;
}

static inline void _set_parent(struct rbnode* node, struct rbnode* p)
{
	long pval = (long)p;
	pval |= node->isblack;
	node->p = (struct rbnode*)pval;
}

static inline struct rbnode* _get_parent(struct rbnode* node)
{
	long pval = (long)node->p;
	pval &= -4;
	return (struct rbnode*)pval;
}

inline void rb_fillnew(struct rbnode* node)
{
	if(node)
	{
		node->key = 0;
		node->isblack = 0;
		node->lchild = 0;
		node->rchild = 0;
		_set_parent(node, 0);
	}
}

inline struct rbnode* rb_parent(struct rbnode* node)
{
	if(!node) goto error_ret;
	return _get_parent(node);
error_ret:
	return 0;
}

inline struct rbnode* rb_sibling(struct rbnode* node)
{
	struct rbnode* p = rb_parent(node);
	if(!p) goto error_ret;

	if(node == l_child(p))
		return r_child(p);
	else if(node == r_child(p))
		return l_child(p);
error_ret:
	return 0;
}

static inline void _left_rotate(struct rbnode* node)
{
	struct rbnode* rc;
	struct rbnode* node_p;

	if(node == 0) goto error_ret;

	rc = node->rchild;
	if(rc == 0) goto error_ret;

	node_p = _get_parent(node);

	_set_parent(rc, node_p);

	if(node_p)
	{
		if(node == node_p->lchild)
			node_p->lchild = rc;
		else
			node_p->rchild = rc;
	}

	_set_parent(node, rc);

	if(rc->lchild)
		_set_parent(rc->lchild, node);

	node->rchild = rc->lchild;

	rc->lchild = node;

error_ret:
	return;
}

static inline void _right_rotate(struct rbnode* node)
{
	struct rbnode* lc;
	struct rbnode* node_p;

	if(node == 0) goto error_ret;

	lc = node->lchild;
	if(lc == 0) goto error_ret;

	node_p = _get_parent(node);

	_set_parent(lc, node_p);

	if(node_p)
	{
		if(node == node_p->lchild)
			node_p->lchild = lc;
		else
			node_p->rchild = lc;
	}

	_set_parent(node, lc);

	if(lc->rchild)
		_set_parent(lc->rchild, node);

	node->lchild = lc->rchild;

	lc->rchild = node;


error_ret:
	return;
}


static void _fix_rr(struct rbtree* t, struct rbnode* node)
{
	struct rbnode* p, *g, *u;
	if(!node) goto error_ret;

	p = _get_parent(node);
	if(!p) goto error_ret;

	while(is_red(p))
	{
		p = _get_parent(node);
		g = _get_parent(p);

		if(p == g->lchild)
		{
			u = g->rchild;
			if(is_red(u))
			{
				// rr-1
				set_black(p);
				set_black(u);
				set_red(g);
				node = g;
				p = _get_parent(node);
			}
			else
			{
				if(node == p->rchild)		// zigzag
				{
					_left_rotate(p);
				}
				else
				{
					node = p;
				}

				set_black(node);
				set_red(g);
				_right_rotate(g);

				break;
			}
		}
		else
		{
			u = g->lchild;
			if(is_red(u))
			{
				// rr-1
				set_black(p);
				set_black(u);
				set_red(g);
				node = g;
				p = _get_parent(node);
			}
			else
			{
				if(node == p->lchild)		// zigzag
				{
					_right_rotate(p);
				}
				else
				{
					node = p;
				}

				set_black(node);
				set_red(g);
				_left_rotate(g);

				break;
			}
		}

	}

	while(node)
	{
		p = _get_parent(node);
		if(!p)
			break;

		node = p;
	}
	t->root = node;
	set_black(t->root);

error_ret:
	return;
}

long rb_insert(struct rbtree* t, struct rbnode* node)
{
	struct rbnode* hot;

	if(!node || ((unsigned long)node & 0x3) != 0) goto error_ret;
	if(rb_search(t, node->key, &hot) != 0) goto error_ret;

	node->isblack = 0;
	node->lchild = 0;
	node->rchild = 0;
	_set_parent(node, hot);

	if(hot != 0)
	{
		if(node->key < hot->key)
			hot->lchild = node;
		else
			hot->rchild = node;

		_set_parent(node, hot);
		_fix_rr(t, node);
	}
	else
		t->root = node;

	t->root->isblack = 1;

	++t->size;
	return 0;
error_ret:
	return -1;
}

struct rbnode* rb_search(struct rbtree* t, unsigned long key, struct rbnode** hot)
{
	struct rbnode* h;
	struct rbnode* p = t->root;

	h = t->root;
	*hot = 0;

	if(!t->root) goto error_ret;

	while(p && p->key != key)
	{
		h = p;
		if(key < p->key)
			p = p->lchild;
		else if(key > p->key)
			p = p->rchild;
	}

	if(h) *hot = h;

	return p;
error_ret:
	return 0;
}

static inline void _link(struct rbnode* p, struct rbnode* lc, struct rbnode* rc)
{
	if(!p) goto error_ret;
	if(p == lc || p == rc) goto error_ret;

	p->lchild = lc;
	p->rchild = rc;

	lc ? _set_parent(lc, p) : 0;
	rc ? _set_parent(rc, p) : 0;

error_ret:
	return;
}

static inline void _transplant(struct rbtree* t, struct rbnode* rm, struct rbnode* sc)
{
	struct rbnode* rm_p;
//	if(!t->root) goto error_ret;
	if(!rm) goto error_ret;

	rm_p = _get_parent(rm);

	if(!rm_p)	 // root
	{
		_link(sc, rm->lchild, rm->rchild);
		t->root = sc;
		if(sc) _set_parent(sc, 0);
	}
	else if(rm == rm_p->lchild)
		_link(rm_p, sc, rm_p->rchild);
	else if(rm == rm_p->rchild)
		_link(rm_p, rm_p->lchild, sc);

	_set_parent(rm, 0);
	rm->lchild = 0;
	rm->rchild = 0;

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
	return 0;
}

static struct rbnode* _rb_min(struct rbnode* x)
{
	while(l_child(x))
	{
		x = l_child(x);
	}

	return x;
}

static struct rbnode* _rb_succ(struct rbnode* x)
{
	struct rbnode* r = r_child(x);
	struct rbnode* y = _get_parent(x);

	if(r)
		return _rb_min(r);
	else
	{
		while(y && x == r_child(y))
		{
			x = y;
			y = _get_parent(y);
		}
	}

	return y;
}

static inline void _swap_node(struct rbnode* r1, struct rbnode* r2)
{
	struct rbnode* p1 = _get_parent(r1);
	struct rbnode* p2 = _get_parent(r2);

	_set_parent(r2, p1);
	_set_parent(r1, p2);

	_swap_value((long*)&(r1->key), (long*)&(r2->key));
	_swap_value((long*)&(r1->lchild), (long*)&(r2->lchild));
	_swap_value((long*)&(r1->rchild), (long*)&(r2->rchild));
}

static inline struct rbnode* _do_remove(struct rbtree* t, struct rbnode* x, struct rbnode** r)
{
	struct rbnode* w = x;

	if(!t->root) goto error_ret;
	if(!x) goto error_ret;

	if(!x->rchild)
	{
		*r = x->lchild;
	}
	else if(!x->lchild)
	{
		*r = x->rchild;
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
	return 0;
}

static void _fix_bb(struct rbtree* tree, struct rbnode* x)
{
	struct rbnode *p, *s, *t;

	if(is_red(x)) goto error_ret;

	while(x)
	{
		p = _get_parent(x);
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
					if(is_red(l_child(s)))
						t = l_child(s);
					else if(is_red(r_child(s)))
						t = r_child(s);

					_right_rotate(p);
					p->isblack ? set_black(s) : set_red(s);
					_cp_color(s, p);
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
					if(is_red(r_child(s)))
						t = r_child(s);
					else if(is_red(l_child(s)))
						t = l_child(s);

					_left_rotate(p);
					_cp_color(s, p);
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
		p = _get_parent(x);
		if(!p)
			break;

		x = p;

		tree->root = x;
	}

	set_black(tree->root);

error_ret:
	return;
}

void rb_remove_node(struct rbtree* t, struct rbnode* x)
{
	struct rbnode* r;
	int isredr, isredx;

	isredx = is_red(x);

	x = _do_remove(t, x, &r);
	isredr = is_red(r);

	if(isredx || isredr)
		set_black(r);
	else
		_fix_bb(t, r);

	set_black(t->root);

	_set_parent(x, 0);
	x->lchild = 0;
	x->rchild = 0;
	
	--t->size;
	if(t->size <= 0)
		t->root = 0;

}

struct rbnode* rb_remove(struct rbtree* t, unsigned long key)
{
	struct rbnode* r;
	struct rbnode* x;
	int isredr, isredx;

	if(!t->root) goto error_ret;
	if(t->size <= 0) return 0;

	x = rb_search(t, key, &r);
	if(!x) goto error_ret;

	rb_remove_node(t, x);

	return x;
error_ret:
	return 0;
}

struct rbt_traverse_node
{
	struct dlnode lstnd;
	struct rbnode* rbnd;
};

struct rbt_free_idx_node
{
	struct dlnode lstnd;
	int i;
};

struct rbt_traverse_node __traverse_pool[MAX_TRAVERSE_QUEUE_LEN];

static int prepare_traverse_node(struct dlist* t_list, struct rbt_traverse_node* nd, struct rbnode* p)
{
	lst_clr(&nd->lstnd);
	nd->rbnd = p;

	if(!lst_push_back(t_list, &nd->lstnd)) goto error_ret;

	return 1;
error_ret:
	return 0;
}

void rb_traverse(struct rbtree* t, order_function f)
{
//	struct rbnode* p = t->root;
//	struct dlist visit_list;
//
//	if(!p) goto error_ret;
//	if(!lst_new(&visit_list)) goto error_ret;
//
//	struct rbt_traverse_node tnd;
//	if(!prepare_traverse_node(&visit_list, &tnd, p)) goto error_ret;
//
//	while(1)
//	{
//		struct dlnode* dln = 0;
//		struct rbt_traverse_node* curnode = 0;
//
//		if(visit_list.size <= 0) break;
//
//		dln = lst_pop_front(&visit_list);
//		if(!dln) goto error_ret;
//
//		curnode = node_cast(rbt_traverse_node, dln, lstnd);
//
//		if(!curnode->rbnd) goto error_ret;
//		(*f)(curnode->rbnd);
//
//		if(curnode->rbnd->lchild != 0)
//		{
//			struct rbt_traverse_node tndl;
//			if(!prepare_traverse_node(&visit_list, &tndl, curnode->rbnd->lchild)) goto error_ret;
//		}
//
//		if(curnode->rbnd->rchild != 0)
//		{
//			struct rbt_traverse_node tndr;
//			if(!prepare_traverse_node(&visit_list, &tndr, curnode->rbnd->rchild)) goto error_ret;
//		}
//	}
//
//	return;
//error_ret:
//	return;
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
