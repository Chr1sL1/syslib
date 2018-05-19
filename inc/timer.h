#ifndef __timer_h__
#define __timer_h__

typedef void(*timer_func_t)(void*);
typedef void* timer_handle_t;

int init_timer(void);

timer_handle_t add_timer(unsigned int delay_tick, timer_func_t callback_func, void* param);

void del_timer(timer_handle_t the_timer);

void on_tick(void);

unsigned long dbg_current_tick(void);

#endif

