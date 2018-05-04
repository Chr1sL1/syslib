#include "timer.h"
#include "linked_list.h"

#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
#endif

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
	int32_t _magic;

	uint64_t _run_tick;
	timer_func_t _callback_func;
	void* _func_param;

	LINKED_LIST_NODE _lln;
};

struct timer_wheel
{
	uint64_t _current_tick;
	uint64_t _current_tv_set_idx;
	CLinkedList _tv_set0[TV_SET0_SIZE];
	CLinkedList _tv_set[TV_SET_CNT][TV_SET_SIZE];
};

static timer_wheel __the_timer_wheel;

static int32_t __log2(unsigned long value)
{
	int32_t ret_code = 0;
	unsigned long idx;

	PROCESS_ERROR(value > 0);

#ifdef _WIN32
	ret_code = _BitScanReverse64(&idx, value);
	LOG_PROCESS_ERROR(ret_code > 0);
#else
	asm("bsrq	%1, %0":"=r"(idx) : "r"(value));
#endif

	return idx;
Exit0:
	return 0;
}

void init_timer(void)
{
	__the_timer_wheel._current_tick = 0;
	__the_timer_wheel._current_tv_set_idx = 0;

	for (int32_t i = 0; i < TV_SET0_SIZE; ++i)
	{
		__the_timer_wheel._tv_set0[i].init();
	}

	for (int32_t i = 0; i < TV_SET_CNT; ++i)
	{
		for (int32_t j = 0; j < TV_SET_SIZE; ++j)
		{
			__the_timer_wheel._tv_set[i][j].init();
		}
	}
}

static void _add_timer(timer_node* tn)
{
	int64_t diff_tick = tn->_run_tick - __the_timer_wheel._current_tick;
	int64_t remain_tick = diff_tick - (TV_SET0_MASK - (__the_timer_wheel._current_tick & TV_SET0_MASK));

	if (remain_tick <= 0)
	{
		__the_timer_wheel._tv_set0[(__the_timer_wheel._current_tick + diff_tick) & TV_SET0_MASK].push_rear(&tn->_lln);
	}
	else
	{
		int32_t tv_set_idx = __log2(remain_tick);

		for (int32_t i = 0; i < TV_SET_CNT; ++i)
		{
			if (tv_set_idx < TV_SET0_BITS + (i + 1) * TV_SET_BITS)
			{
				int32_t idx = remain_tick / (1UL << (TV_SET0_BITS + i * TV_SET_BITS));
				__the_timer_wheel._tv_set[i][(__the_timer_wheel._current_tv_set_idx + idx) & TV_SETN_MASK].push_rear(&tn->_lln);
				break;
			}
		}
	}
}


timer_handle_t add_timer(int32_t delay_tick, timer_func_t callback_func, void* param)
{
	timer_node* _node = NULL;

	LOG_PROCESS_ERROR(delay_tick > 0);
	LOG_PROCESS_ERROR(callback_func);

	_node = (timer_node*)malloc(sizeof(timer_node));

	_node->_magic = TIMER_NODE_TYPE_MAGIC;
	_node->_run_tick = __the_timer_wheel._current_tick + delay_tick;
	_node->_callback_func = callback_func;
	_node->_func_param = param;
	clear_list_node(&_node->_lln);

	_add_timer(_node);

	return _node;
Exit0:
	return NULL;
}

static void _del_timer(timer_node* tn)
{
	CLinkedList::remove(&tn->_lln);
	free(tn);
}

void del_timer(timer_handle_t the_timer)
{
	timer_node* _node = (timer_node*)the_timer;
	LOG_PROCESS_ERROR(_node);
	LOG_PROCESS_ERROR(_node->_magic == TIMER_NODE_TYPE_MAGIC);

	_del_timer(_node);

	return;
Exit0:
	return;
}

static void _casade(void)
{
	LINKED_LIST_NODE* node = __the_timer_wheel._tv_set[0][__the_timer_wheel._current_tv_set_idx].head().pNext;
	while (node != &__the_timer_wheel._tv_set[0][__the_timer_wheel._current_tv_set_idx].rear())
	{
		LINKED_LIST_NODE* cur_node = node;
		node = node->pNext;

		timer_node* tn = (timer_node*)((uint64_t)cur_node - (uint64_t)&((timer_node*)(0))->_lln);

		CLinkedList::remove(cur_node);

		__the_timer_wheel._tv_set0[(tn->_run_tick - __the_timer_wheel._current_tick - 1) & TV_SET0_MASK].push_rear(cur_node);
	}

	for (int32_t i = 1; i < TV_SET_CNT; ++i)
	{
		LINKED_LIST_NODE* node = __the_timer_wheel._tv_set[i][__the_timer_wheel._current_tv_set_idx].head().pNext;
		while (node != &__the_timer_wheel._tv_set[i][__the_timer_wheel._current_tv_set_idx].rear())
		{
			LINKED_LIST_NODE* cur_node = node;
			node = node->pNext;

			timer_node* tn = (timer_node*)((uint64_t)cur_node - (uint64_t)&((timer_node*)(0))->_lln);

			CLinkedList::remove(cur_node);

			__the_timer_wheel._tv_set[i - 1][(tn->_run_tick - __the_timer_wheel._current_tick) / (1UL << (TV_SET0_BITS + (i - 1) * TV_SET_BITS)) + 1].push_rear(cur_node);
		}
	}
}

void on_tick(void)
{
	LINKED_LIST_NODE* node = __the_timer_wheel._tv_set0[__the_timer_wheel._current_tick & TV_SET0_MASK].head().pNext;
	while (node != &__the_timer_wheel._tv_set0[__the_timer_wheel._current_tick & TV_SET0_MASK].rear())
	{
		LINKED_LIST_NODE* cur_node = node;
		node = node->pNext;

		timer_node* tn = (timer_node*)((uint64_t)cur_node - (uint64_t)&((timer_node*)(0))->_lln);

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

uint64_t dbg_current_tick(void)
{
	return __the_timer_wheel._current_tick;
}
