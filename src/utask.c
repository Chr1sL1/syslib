#include "utask.h"
#include "rbtree.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>

#define UTASK_MAGIC_NUM	 0x1234567887654321

enum __utask_state
{
	uts_invalid = 0,

	uts_inited,
	uts_waiting,
	uts_running,
	uts_finished,

	uts_total,
};

struct __reg_values
{
	unsigned long rax;
	unsigned long rbx;
	unsigned long rcx;
	unsigned long rdx;
	unsigned long rsi;
	unsigned long rdi;
	unsigned long rsp;
	unsigned long rbp;
	unsigned long rip;

	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;
	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;
};

struct __utask
{
	struct utask _utsk;

	long _task_state;
	void* _yield_pos;
	void* _next_rsp;
	long _jmp_flag;
	void* _resume_pos;
	void* _resume_running_pos;

	struct __reg_values _saved_regs;
	long _magic_number;
	struct rbnode _rbnode;
};

extern void asm_run_task(struct __utask* tsk, void* udata);
extern void asm_yield_task(struct __utask* tsk);
extern void asm_resume_task(struct __utask* tsk);

static struct __utask* _conv_task(struct utask* tsk)
{
	struct __utask* itsk = 0;
	if(!tsk) goto error_ret;

	itsk = (struct __utask*)tsk;
	if(itsk->_magic_number != UTASK_MAGIC_NUM) goto error_ret;

	return itsk;
error_ret:
	return 0;
}

struct utask* make_task(void* stack_ptr, long stack_size, task_function tfunc)
{
	struct __utask* tsk = (struct __utask*)malloc(sizeof(struct __utask));

	if(!tsk) goto error_ret;
	if(!stack_ptr) goto error_ret;
	if(stack_size <= 0) goto error_ret;
	if(!tfunc) goto error_ret;

	memset(tsk, 0, sizeof(struct __utask));
	tsk->_utsk._stack = stack_ptr;
	tsk->_utsk._stack_size = stack_size;
	tsk->_utsk._func = tfunc;
	tsk->_magic_number = UTASK_MAGIC_NUM;
	tsk->_task_state = uts_inited;

	return &tsk->_utsk;
error_ret:
	return 0;
}

void del_task(struct utask* tsk)
{
	struct __utask* t = _conv_task(tsk);
	if(!t) goto error_ret;

	free(t);
error_ret:
	return;
}

int run_task(struct utask* tsk, void* udata)
{
	struct __utask* itsk = _conv_task(tsk);
	if(itsk->_task_state != uts_inited) goto error_ret;

	itsk->_task_state = uts_running; 

	asm_run_task(itsk, udata);

	return 1;
error_ret:
	return 0;
}

int yield_task(struct utask* tsk)
{
	struct __utask* itsk = _conv_task(tsk);
	if(itsk->_task_state != uts_running) goto error_ret;

	itsk->_task_state = uts_waiting; 

	asm_yield_task(itsk);

	return 1;
error_ret:
	return 0;
}

int resume_task(struct utask* tsk)
{
	struct __utask* itsk = _conv_task(tsk);
	if(itsk->_task_state != uts_waiting) goto error_ret;

	itsk->_task_state = uts_running; 
	asm_resume_task(itsk);

	return 1;
error_ret:
	return 0;
}

