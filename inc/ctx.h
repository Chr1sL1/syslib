#ifndef __ctx_h__
#define __ctx_h__

#include <ucontext.h>

#define USR_STACK_SIZE	(4 * 1024 * 1024)

typedef void (*task_fn_t)(void*);

struct usr_task
{
	ucontext_t _ucp;

	char _usr_stack[USR_STACK_SIZE];
	task_fn_t _usr_fn;
	void* _fn_param;
};

int new_task(struct usr_task* tsk, task_fn_t fn, void* param);
int run_task(struct usr_task* tsk);
int yield_task(struct usr_task* tsk);
void del_task(struct usr_task* tsk);


#endif