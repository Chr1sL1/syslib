#ifndef __utask_h__
#define __utask_h__


struct utask;

typedef void (*task_function)(struct utask*, void*);

struct utask
{
	void* _stack;
	long _stack_size;
	task_function _func;
};

// for temp use
struct utask* make_task(void* stackptr, long stacksize, task_function tfunc);
void del_task(struct utask* tsk);
//

int run_task(struct utask* tsk, void* udata);
int yield_task(struct utask* tsk);
int resume_task(struct utask* tsk);

#endif	// __utask_h__
