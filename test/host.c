#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>

#include "shmem.h"
#include "rbtree.h"
#include "dlist.h"
#include "graph.h"
#include "common.h"

const char* share_memory_name = "test_shm_17x";

struct ucontext ctx1;
struct ucontext ctx2;
struct ucontext ctx_default;

char stk1[16384];
char stk2[16384];
char stk_default[16384];
int test_arr[100];

struct ad_test
{
	int _value1;
	int _value2;
};

#pragma comment(push,1)
struct ut
{
	union
	{
		int* ptr;
		struct 
		{
			unsigned long ptr63 : 63;
			unsigned char clr : 1;
		};
	};
};

struct utt
{
	union
	{
		unsigned long v;
		struct
		{
			unsigned char a;
			unsigned char b;
			unsigned char c;
			unsigned char d;
			unsigned char e;
			unsigned char f;
			unsigned char g;
			unsigned char h;
		};

	};
};

#pragma comment(pop)



//void test_link(void)
//{
//	struct ad_test __test;
//	printf("_head addr: %p\n", &__test._head);
//
//	__test._value1 = 746;
//	__test._value2 = 626;
//
//	struct ad_test* p = (struct ad_test*)((void*)&__test._head - (void*)&((struct ad_test*)0)->_head);
//
//	printf("off head addr: %p\n", &((struct ad_test*)0)->_head);
//	printf("actual addr: %p\n", &__test);
//	printf("p addr: %p\n", p);
//	printf("%d,%d\n", p->_value1, p->_value2);
//
//}
//
//
void swap(int* a, int* b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

void print_array(int* a, int n)
{
	for(int i = 0; i < n; ++i)
	{
		printf("%d,", a[i]);
	}
	printf("\n");
}

void random_shuffle(int* a, int n)
{
	for(int i = 0; i < n; ++i)
	{
		int rd = random() % n;
		swap(&a[i], &a[rd]);
	}
}


void print_node(struct rbnode* node)
{
	if(node)
	{
		struct rbnode* lc = node->lchild;
		struct rbnode* rc = node->rchild;

//		printf("%d(%d)[%d,%d] ", node->key, node->isblack, lc ? lc->key:0, rc ? rc->key:0);
	}

}

void test_rbtree(void)
{
	int test_arr_count= sizeof(test_arr) / sizeof(int);
	struct timeval tv_begin, tv_end;

	for(int i = 0; i < test_arr_count; i++) 
		test_arr[i] = i;

//	random_shuffle(test_arr, test_arr_count);

	struct rbtree test_tree;
	test_tree.size = 0;
	test_tree.root = NULL;

	gettimeofday(&tv_begin, NULL);

	for(int i = 0; i < test_arr_count; i++)
	{
		struct rbnode* node = (struct rbnode*)malloc(sizeof(struct rbnode));
		rb_fillnew(node);

		node->key = test_arr[i];

		rb_insert(&test_tree, node);

//		pre_order(test_tree.root, print_node);
//		printf("\n");
//		in_order(test_tree.root, print_node);
		printf("\n---size %d---\n", test_tree.size);
	}

	gettimeofday(&tv_end, NULL);
	printf("insert elapse: %ld.\n", (long)tv_end.tv_usec - (long)tv_begin.tv_usec);
	printf("rooooooooooooooooooot:%ld\n", test_tree.root->key);

	printf("traverse: \n");
	rb_traverse(&test_tree, print_node);	
	printf("\n");

	random_shuffle(test_arr, test_arr_count);

	gettimeofday(&tv_begin, NULL);

	for(int i = 0; i < test_arr_count; i++)
	{
		struct rbnode* node = rb_remove(&test_tree, test_arr[i]);

		if(node)
		{
//			pre_order(test_tree.root, print_node);
//			printf("\n");
//			in_order(test_tree.root, print_node);
			printf("\n---size %d---\n", test_tree.size);

			free(node);
			node = NULL;
		}
	}

	gettimeofday(&tv_end, NULL);
	printf("delete elapse: %ld.\n", (long)tv_end.tv_usec - (long)tv_begin.tv_usec);
}

void test_lst(void)
{
	struct kt
	{
		int value;
		struct dlnode nd;
	};

	struct dlist lst;
	struct kt* k = NULL;
	lst_new(&lst);
	int test_arr_count= sizeof(test_arr) / sizeof(int);

	for(int i = 0; i < test_arr_count; i++) 
		test_arr[i] = i;

	random_shuffle(test_arr, test_arr_count);

	for(int i = 0; i < test_arr_count; ++i)
	{
		k = malloc(sizeof(struct kt));
		lst_clr(&k->nd);
		k->value = test_arr[i];
		lst_push_back(&lst, &k->nd);

		printf("%d,", k->value);
	}

	printf("\n");

	struct dlnode * it = lst.head.next;
	while(it != &lst.tail)
	{
		struct kt* k = (struct kt*)((void*)it - (void*)(&((struct kt*)0)->nd));
		printf("%d,", k->value);

		it = it->next;
	}

	printf("\n");

	for(int i = 0; i < test_arr_count; ++i)
	{
		struct dlnode* n = lst_pop_front(&lst);
		struct kt* k = node_cast(kt, n, nd);
		printf("%d,", k->value);
	}
	printf("\n");
}

void func1(char* param)
{
	printf("func1 executed.%s\n", param);
	if(swapcontext(&ctx1, &ctx2) < 0)
		goto error_ret;

	printf("func1 return.\n");
	return;
error_ret:
	printf("func1 error.\n");
	return;
}

void func2(void)
{
	printf("func2 executed.\n");

	if(swapcontext(&ctx2, &ctx1) < 0)
		goto error_ret;

	printf("func2 return.\n");
	return;
error_ret:
	printf("func2 error.\n");
}

void func_default(void)
{
	while(1)
	{
		printf("waiting...\n");
		sleep(1);
	}
}


//void test_ctx(void)
//{
//	if(getcontext(&ctx1) < 0)
//		goto error_ret;
//
//	ctx1.uc_stack.ss_sp = stk1;
//	ctx1.uc_stack.ss_size = sizeof(stk1);
//	ctx1.uc_link = &ctx2;
//	makecontext(&ctx1, func1, 1, (int)(share_memory_name)); 
//
//	if(getcontext(&ctx2) < 0)
//		goto error_ret;
//
//	ctx2.uc_stack.ss_sp = stk2;
//	ctx2.uc_stack.ss_size = sizeof(stk2);
//	ctx2.uc_link = &ctx_default;
//	makecontext(&ctx2, func2, 0);
//
//	if(getcontext(&ctx_default) < 0)
//		goto error_ret;
//
//	ctx_default.uc_stack.ss_sp = stk_default;
//	ctx_default.uc_stack.ss_size = sizeof(stk_default);
//	ctx_default.uc_link = NULL;
//	makecontext(&ctx_default, func_default, 0);
//
//	printf("test_stk started.\n");
//
//	//	if(swapcontext(&ctx_default, &ctx1) < 0)
//	if(setcontext(&ctx1) < 0)
//		goto error_ret;
//	printf("test_stk finished.\n");
//
//	return;
//error_ret:
//	perror("fk..\n");
//	return;
//}

void insertion_sort(int* a, int count)
{
	for(int i = 0; i < count; i++)
	{
		for(int j = i + 1; j < count; j++)
		{
			if(a[j] < a[i])
			{
				int tmp = a[j];
				a[j] = a[i];
				a[i] = tmp;
			}
		}
	}
}

int main(void)
{
	//	test_link();
	//
	//	struct shm_host_ptr* __ptr = shmem_new(share_memory_name, 1024 * 1024 * 1024);
	//	struct shm_client_ptr* __read_ptr = NULL;
	//
	//	if(!__ptr)
	//		return -1;
	//
	//	sprintf((char*)__ptr->_the_addr._addr, "hello world.");
	//
	//	__read_ptr = shmem_open(share_memory_name);
	//
	//	if(!__read_ptr)
	//		return -1;
	//
	//	printf("content: %s\n", (char*)__read_ptr->_the_addr._addr);
	//	printf("pid: %u, groupid: %u\n", getpid(), getpgrp());
	//
	//
	//	shmem_close(__read_ptr);
	//	shmem_destroy(__ptr);
	//
	//	test_lst();
	//	dd
	//
	//	test_ctx();
	//
	//


	//	struct utt _utt;
	//	_utt.v = 0x0102030405060708;
	//
	//	printf("%x\n", _utt.a);
	//	printf("%x\n", _utt.b);
	//	printf("%x\n", _utt.c);
	//	printf("%x\n", _utt.d);
	//	printf("%x\n", _utt.e);
	//	printf("%x\n", _utt.f);
	//	printf("%x\n", _utt.g);
	//	printf("%x\n", _utt.h);
	//
	//
	//
	//	int a = 0x1234ABCD;
	//	struct ut u;
	//	u.ptr = &a;
	//	printf("%.16p\n", u.ptr);
	//	printf("%.16p\n", (int*)u.ptr63);
	//
	//	u.clr |= 0x8000000000000000;
	//	printf("%d\n", u.clr);
	//
	//	u.clr &= ~0x8000000000000000;
	//	printf("%x\n", *(int*)(u.ptr63));

	test_rbtree(); 
//	test_lst();

	return 0;
}
