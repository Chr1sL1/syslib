#ifndef __timer_h__
#define __timer_h__

struct timer_node
{
	unsigned long start_time;
	unsigned long interval;
	void (*on_timeer_func)(struct timer_node* tn);
};

#endif

