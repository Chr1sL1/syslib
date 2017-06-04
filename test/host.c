#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "shmem.h"
#include "rbtree.h"
#include "dlist.h"
#include "graph.h"
#include "common.h"
#include "utask.h"

const char* share_memory_name = "test_shm_17x";

int test_arr[100];

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

void tfun(struct utask* t)
{
	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
		printf("arr[i] = %d\n", test_arr[i]);

		if(i % 2 == 0)
			yield_task(t);

		sleep(1);
	}
}

void test_task(void)
{
	void* ustack = malloc(1024);
	struct utask* tsk = make_task(ustack, 1024, tfun);
	if(!tsk) goto error_ret;

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
		test_arr[i] = i;
	}

	run_task(tsk, 0);

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
		printf("%d\n", test_arr[i]);
		if(i % 3 == 0)
			resume_task(tsk);

		sleep(1);
	}

error_ret:
	free(ustack);
	return;
}

int main(void)
{
//	test_task();

	unsigned int aaa = align_to_2power(14);

//	test_rbtree(); 
//	test_lst();

	return 0;
}
