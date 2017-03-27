#include "ctx.h"

int new_task(struct usr_task* tsk, task_fn_t fn, void* param)
{
	if(!tsk)
		goto error_ret;

	if(getcontext(&tsk->_ucp) < 0)
		goto error_ret;

	tsk->_ucp.uc_stack.ss_sp = tsk->_usr_stack;
	tsk->_ucp.uc_stack.ss_size = sizeof(tsk->_usr_stack);

	tsk->_usr_fn = fn;
	tsk->_fn_param = param;

//	if(makecontext(&tsk->_ucp, tsk->_usr_fn, tsk->_fn_param) < 0)
	//	goto error_ret;

	return 1;
error_ret:
	return 0;
}

int run_task(struct usr_task* tsk)
{
	if(!tsk)
		goto error_ret;
	return 1;
error_ret:
	return 0;
}

int yield_task(struct usr_task* tsk)
{
	if(!tsk)
		goto error_ret;
	return 1;
error_ret:
	return 0;
}

void del_task(struct usr_task* tsk)
{
	if(!tsk)
		goto error_ret;

error_ret:
	return;
}
