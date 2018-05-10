#include "timer.h"
#include "dlist.h"
#include "misc.h"
#include "mmspace.h"
#include <stdio.h>


#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
#endif

#define TIMER_NODE_ZONE_NAME "timer_node_zone"

#define TIMER_NODE_TYPE_MAGIC	(0x12341234)

#define TV_SET0_BITS	(8)
#define TV_SET_BITS		(6)

#define TV_SET0_SIZE	(1UL << TV_SET0_BITS)
#define TV_SET_SIZE		(1UL << TV_SET_BITS)

#define TV_SET0_MASK	((1UL << TV_SET0_BITS) - 1)
#define TV_SET_MASK(N)	((1UL << (TV_SET0_BITS + N * TV_SET_BITS)) - 1)
#define TV_SETN_MASK	((1UL << TV_SET_BITS) - 1)

#define TV_SET_CNT		(4)

struct timer_node
{
	int _magic;

	unsigned long _run_tick;
	timer_func_t _callback_func;
	void* _func_param;

	struct dlnode _lln;
};

struct timer_wheel
{
	unsigned long _current_tick;
	unsigned long _current_tv_set_idx;
	struct dlist _tv_set0[TV_SET0_SIZE];
	struct dlist _tv_set[TV_SET_CNT][TV_SET_SIZE];
};

static struct timer_wheel __the_timer_wheel;
static struct mmcache* __the_timer_node_zone = 0;

static inline int _timer_node_try_restore_zone(void)
{
	if(!__the_timer_node_zone)
	{
		__the_timer_node_zone = mm_search_zone(TIMER_NODE_ZONE_NAME);
		if(!__the_timer_node_zone)
		{
			__the_timer_node_zone = mm_cache_create(TIMER_NODE_ZONE_NAME, sizeof(struct timer_node), 0, 0);
			if(!__the_timer_node_zone) goto error_ret;
		}
		else
		{
			if(__the_timer_node_zone->obj_size != sizeof(struct timer_node))
				goto error_ret;
		}
	}

	return 0;
error_ret:
	return -1;

}

int init_timer(void)
{
	err_exit(_timer_node_try_restore_zone() < 0, "init_timer: restore zone failed.");

	__the_timer_wheel._current_tick = 0;
	__the_timer_wheel._current_tv_set_idx = 0;

	for (int i = 0; i < TV_SET0_SIZE; ++i)
	{
		lst_new(&__the_timer_wheel._tv_set0[i]);
	}

	for (int i = 0; i < TV_SET_CNT; ++i)
	{
		for (int j = 0; j < TV_SET_SIZE; ++j)
		{
			lst_new(&__the_timer_wheel._tv_set[i][j]);
		}
	}

	return 0;
error_ret:
	return -1;
}

static void _add_timer(struct timer_node* tn)
{
	long diff_tick = tn->_run_tick - __the_timer_wheel._current_tick;
	long remain_tick = diff_tick - (TV_SET0_MASK - (__the_timer_wheel._current_tick & TV_SET0_MASK));

	if (remain_tick <= 0)
	{
		lst_push_back(&__the_timer_wheel._tv_set0[(__the_timer_wheel._current_tick + diff_tick) & TV_SET0_MASK], &tn->_lln);
	}
	else
	{
		int tv_set_idx = log_2(remain_tick);

		for (int i = 0; i < TV_SET_CNT; ++i)
		{
			if (tv_set_idx < TV_SET0_BITS + (i + 1) * TV_SET_BITS)
			{
				int idx = remain_tick / (1UL << (TV_SET0_BITS + i * TV_SET_BITS));
				lst_push_back(&__the_timer_wheel._tv_set[i][(__the_timer_wheel._current_tv_set_idx + idx) & TV_SETN_MASK], &tn->_lln);
				break;
			}
		}
	}
}


timer_handle_t add_timer(int delay_tick, timer_func_t callback_func, void* param)
{
	struct timer_node* _node = NULL;

	err_exit(delay_tick <= 0, "add_timer: illegal param.");
	err_exit(!callback_func, "add_timer: illegal param.");

	_node = mm_cache_alloc(__the_timer_node_zone);
	err_exit(!_node, "add_timer: alloc timer node failed.");

	_node->_magic = TIMER_NODE_TYPE_MAGIC;
	_node->_run_tick = __the_timer_wheel._current_tick + delay_tick;
	_node->_callback_func = callback_func;
	_node->_func_param = param;
	lst_clr(&_node->_lln);

	_add_timer(_node);

	return _node;
error_ret:
	return 0;
}

static void _del_timer(struct timer_node* tn)
{
	lst_remove_node(&tn->_lln);
	mm_cache_free(__the_timer_node_zone, tn);
}

void del_timer(timer_handle_t the_timer)
{
	struct timer_node* _node = (struct timer_node*)the_timer;
	err_exit(!_node, "del_timer null node.");
	err_exit(_node->_magic != TIMER_NODE_TYPE_MAGIC, "del_timer type error.");

	_del_timer(_node);

	return;
error_ret:
	return;
}

static void _casade(void)
{
	struct dlnode* node = lst_first(&__the_timer_wheel._tv_set[0][__the_timer_wheel._current_tv_set_idx]);
	while (node != lst_last(&__the_timer_wheel._tv_set[0][__the_timer_wheel._current_tv_set_idx]))
	{
		struct dlnode* cur_node = node;
		node = node->next;

		struct timer_node* tn = (struct timer_node*)((unsigned long)cur_node - (unsigned long)&((struct timer_node*)(0))->_lln);

		lst_remove(&__the_timer_wheel._tv_set[0][__the_timer_wheel._current_tv_set_idx], cur_node);

		lst_push_back(&__the_timer_wheel._tv_set0[(tn->_run_tick - __the_timer_wheel._current_tick - 1) & TV_SET0_MASK], cur_node);
	}

	for (int i = 1; i < TV_SET_CNT; ++i)
	{
		struct dlnode* node = lst_first(&__the_timer_wheel._tv_set[i][__the_timer_wheel._current_tv_set_idx]);
		while (node != lst_last(&__the_timer_wheel._tv_set[i][__the_timer_wheel._current_tv_set_idx]))
		{
			struct dlnode* cur_node = node;
			node = node->next;

			struct timer_node* tn = (struct timer_node*)((unsigned long)cur_node - (unsigned long)&((struct timer_node*)(0))->_lln);

//			CLinkedList::remove(cur_node);
			lst_remove(&__the_timer_wheel._tv_set[i][__the_timer_wheel._current_tv_set_idx], cur_node);

			lst_push_back(&__the_timer_wheel._tv_set[i - 1][(tn->_run_tick - __the_timer_wheel._current_tick) / (1UL << (TV_SET0_BITS + (i - 1) * TV_SET_BITS)) + 1], cur_node);
		}
	}
}

void on_tick(void)
{
	struct dlnode* node = lst_first(&__the_timer_wheel._tv_set0[__the_timer_wheel._current_tick & TV_SET0_MASK]);
	while (node != lst_last(&__the_timer_wheel._tv_set0[__the_timer_wheel._current_tick & TV_SET0_MASK]))
	{
		struct dlnode* cur_node = node;
		node = node->next;

		struct timer_node* tn = (struct timer_node*)((unsigned long)cur_node - (unsigned long)&((struct timer_node*)(0))->_lln);

		(*tn->_callback_func)(tn->_func_param);

		_del_timer(tn);
	}

	if ((__the_timer_wheel._current_tick & TV_SET0_MASK) == TV_SET0_MASK)
	{
		_casade();
		__the_timer_wheel._current_tv_set_idx = (++__the_timer_wheel._current_tv_set_idx & TV_SETN_MASK);
	}

	++__the_timer_wheel._current_tick;
}

unsigned long dbg_current_tick(void)
{
	return __the_timer_wheel._current_tick;
}