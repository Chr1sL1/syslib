#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ucontext.h>

#include "shmem.h"
#include "rbtree.h"
#include "dlist.h"
#include "graph.h"

const char* share_memory_name = "test_shm_17x";

struct ucontext ctx1;
struct ucontext ctx2;
struct ucontext ctx_default;

char stk1[16384];
char stk2[16384];
char stk_default[16384];

struct ad_test
{
	int _value1;
	int _value2;
};

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

		printf("%d(%d)[%d,%d] ", node->key, node->isblack, lc ? lc->key:0, rc ? rc->key:0);
	}

}

void test_rbtree(void)
{
//	int test_arr[] = { 3,2,5,4,6,8,7,9,1,0,11,12,13,14,15,16,17,18,19,10 };
//	int test_arr[] = { 10,9,8,7,6,5,4,3,2,1 };
//	int test_arr_rand[] = { 3,5,4,2,7,9,6,10,1,8 };


	int test_arr[100];
	int test_arr_count= sizeof(test_arr) / sizeof(int);

	for(int i = 0; i < test_arr_count; i++) 
		test_arr[i] = i;


	random_shuffle(test_arr, test_arr_count);

	struct rbtree test_tree;
	test_tree.size = 0;
	test_tree.root = NULL;

	for(int i = 0; i < test_arr_count; i++)
	{
		struct rbnode* node = (struct rbnode*)malloc(sizeof(struct rbnode));
		rb_fillnew(node);

		node->key = test_arr[i];

		rb_insert(&test_tree, node);

		pre_order(test_tree.root, print_node);
		printf("\n");
		in_order(test_tree.root, print_node);
		printf("\n---size %d---\n", test_tree.size);
	}

	printf("rooooooooooooooooooot:%d\n", test_tree.root->key);

	random_shuffle(test_arr, test_arr_count);

	for(int i = 0; i < test_arr_count; i++)
	{
		struct rbnode* node = rb_remove(&test_tree, test_arr[i]);

		if(node)
		{
			pre_order(test_tree.root, print_node);
			printf("\n");
			in_order(test_tree.root, print_node);
			printf("\n---size %d---\n", test_tree.size);

			free(node);
			node = NULL;
		}
	}
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

	for(int i = 0; i < 10; ++i)
	{
		k = malloc(sizeof(struct kt));
		lst_clr(&k->nd);
		k->value = i;

		lst_insert_after(&lst, &lst.head, &k->nd);
	}

	struct dlnode * it = lst.head.next;
	while(it != &lst.tail)
	{
		struct kt* k = (struct kt*)((void*)it - (void*)(&((struct kt*)0)->nd));
		printf("%d,", k->value);

		it = it->next;
	}
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


void test_ctx(void)
{
	if(getcontext(&ctx1) < 0)
		goto error_ret;

	ctx1.uc_stack.ss_sp = stk1;
	ctx1.uc_stack.ss_size = sizeof(stk1);
	ctx1.uc_link = &ctx2;
	makecontext(&ctx1, func1, 1, (int)(share_memory_name)); 

	if(getcontext(&ctx2) < 0)
		goto error_ret;

	ctx2.uc_stack.ss_sp = stk2;
	ctx2.uc_stack.ss_size = sizeof(stk2);
	ctx2.uc_link = &ctx_default;
	makecontext(&ctx2, func2, 0);

	if(getcontext(&ctx_default) < 0)
		goto error_ret;

	ctx_default.uc_stack.ss_sp = stk_default;
	ctx_default.uc_stack.ss_size = sizeof(stk_default);
	ctx_default.uc_link = NULL;
	makecontext(&ctx_default, func_default, 0);

	printf("test_stk started.\n");

//	if(swapcontext(&ctx_default, &ctx1) < 0)
	if(setcontext(&ctx1) < 0)
		goto error_ret;
	printf("test_stk finished.\n");

	return;
error_ret:
	perror("fk..\n");
	return;
}

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

	test_rbtree(); 
	return 0;
}
