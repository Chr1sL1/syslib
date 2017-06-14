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
#include "misc.h"
#include "mmpool.h"

const char* share_memory_name = "test_shm_17x";

int test_arr[100];

char* mmp_buf;

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
	unsigned long r1 = 0, r2 = 0;
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

		r1 = rdtsc();
		rb_insert(&test_tree, node);
		r2 = rdtsc();
		printf("rbinsert: %lu cycles.", r2 - r1);

//		pre_order(test_tree.root, print_node);
//		printf("\n");
//		in_order(test_tree.root, print_node);
		printf("\n---size %d---\n", test_tree.size);
	}

	gettimeofday(&tv_end, NULL);
	printf("insert elapse: %ld.\n", (long)tv_end.tv_usec - (long)tv_begin.tv_usec);
	printf("rooooooooooooooooooot:%ld\n", test_tree.root->key);

//	printf("traverse: \n");
//	rb_traverse(&test_tree, print_node);	
//	printf("\n");

	random_shuffle(test_arr, test_arr_count);

	gettimeofday(&tv_begin, NULL);

	for(int i = 0; i < test_arr_count; i++)
	{
		r1 = rdtsc();
		struct rbnode* node = rb_remove(&test_tree, test_arr[i]);
		r2 = rdtsc();
		printf("rbremove: %lu cycles.", r2 - r1);

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

void tfun(struct utask* t, void* p)
{
	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
//		printf("arr[i] = %d\n", test_arr[i]);

		if(i % 2 == 0)
			yield_task(t);

//		sleep(1);
	}
}

void test_task(void)
{
	unsigned long r1 = 0, r2 = 0;
	void* ustack = malloc(1024);
	struct utask* tsk = make_task(ustack, 1024, tfun);
	if(!tsk) goto error_ret;

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
		test_arr[i] = i;
	}

	r1 = rdtsc();
	run_task(tsk, 0);
	r2 = rdtsc();
	printf("runtask: %lu cycles.\n", r2 - r1);

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
//		printf("%d\n", test_arr[i]);
		if(i % 3 == 0)
		{
			r1 = rdtsc();
			resume_task(tsk);
			r2 = rdtsc();
			printf("resume: %lu cycles.\n", r2 - r1);
		}

//		sleep(1);
	}

error_ret:
	free(ustack);
	return;
}

long test_mmpool(void)
{
	long rslt = 0;
	unsigned int size = 100 * 1024 * 1024 + 16;
	long rnd = 0;
	unsigned long r1 = 0, r2 = 0;

	mmp_buf = malloc(size);

	struct mmpool* pool = mmp_new(mmp_buf, size);

	if(!pool) goto error_ret;

	for(long i = 0; i < 100; i++)
	{
		rnd = random() % 65535;

		r1 = rdtsc();
		void* p = mmp_alloc(pool, rnd);
		r2 = rdtsc();

		printf("alloc cycles: %lu\n", r2 - r1);

		if(!p) printf("alloc errrrrrrrrrrrrrrrrror.\n");

		r1 = rdtsc();
		mmp_free(pool, p);
		r2 = rdtsc();
		printf("free cycles: %lu\n", r2 - r1);

		rslt = mmp_check(pool);
		if(rslt < 0)
			goto error_ret;
	}

	rslt = mmp_check(pool);

	mmp_del(pool);

error_ret:
	return -1;
}

unsigned int at2f(unsigned v)
{
	int i = 32;
	unsigned int k = 0;

	for(; i >= 0; --i)
	{
		k = (1 << i);
		if((k & v) != 0)
			break;
	}

	return k;
}

unsigned int at2t(unsigned v)
{
	int i = 32;
	unsigned int k = 0;

	for(; i >= 0; --i)
	{
		k = (1 << i);
		if((k & v) != 0)
			break;
	}

	return (1 << (k + 1));
}

int main(void)
{
	unsigned long i = align_to_2power_floor(0);
	printf("%lu\n", i);

	srandom(25234978);
	test_mmpool();

//	unsigned long r1 = rdtsc();
//	unsigned int aaa = align_to_2power_top(11);
//	unsigned long r2 = rdtsc();
//	printf("aaa = %u\n", aaa);
//	printf("cycle = %lu\n", r2 - r1);
//
//	r1 = rdtsc();
//	aaa = at2t(11);
//	r2 = rdtsc();
//	printf("aaa = %u\n", aaa);
//	printf("cycle = %lu\n", r2 - r1);
//
//
//	test_rbtree(); 
//	test_task();
//	test_lst();

	return 0;
}
