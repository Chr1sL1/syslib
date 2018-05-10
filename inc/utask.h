#ifndef __utask_h__
#define __utask_h__


struct utask;

typedef void (*task_function)(struct utask*, void*);

struct utask
{
	void* stk;
	long stk_size;
	task_function tsk_func;
};

struct utask* utsk_create(task_function tfunc);
void utsk_destroy(struct utask* tsk);
//

int utsk_run(struct utask* tsk, void* udata);
int utsk_yield(struct utask* tsk);
int utsk_resume(struct utask* tsk);

#endif	// __utask_h__
