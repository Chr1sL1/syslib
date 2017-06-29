#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "shmem.h"
#include "rbtree.h"
#include "dlist.h"
#include "graph.h"
#include "common.h"
#include "utask.h"
#include "misc.h"
#include "mmpool.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>

const char* share_memory_name = "test_shm_17x";

int test_arr[100];

char* mmp_buf;

long running = 0;

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

void signal_stop(int sig, siginfo_t* t, void* usr_data)
{
	struct mmpool* mp = (struct mmpool*)usr_data;
	printf("------------------recvd signal------------------------\n");
	mmp_freelist_profile(mp);
	printf("------------------signal end------------------------\n");
	running = 0;
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

struct mmp_test_entry
{
	void* _block;
	long _size;
	long _usage_duration;
	long _alloc_time;
};

long test_mmp(long total_size, long min_block_idx, long max_block_idx, long node_count)
{
	long rslt = 0;
	void* mm_buf = 0;
	unsigned long now_time = 0;
	unsigned long loop_count = 0;
	unsigned long restart_alloc_time = 0;
	long req_total_size = 0;
	long min_block_size, max_block_size;
	long enable_alloc = 1;
	struct mmpool* mp = 0;
	struct mmp_test_entry te[node_count];
	struct timeval tv;

	struct mmpool_config cfg;
	cfg.min_block_index = min_block_idx;
	cfg.max_block_index = max_block_idx;

	mmp_buf = malloc(total_size);
	if(!mmp_buf) goto error_ret;

	mp = mmp_new(mmp_buf, total_size, &cfg);
	if(!mp) goto error_ret;

	min_block_size = 1 << min_block_idx;
	max_block_size = 1 << max_block_idx;

	for(long i = 0; i < node_count; ++i)
	{
		te[i]._size = random() % (max_block_size - 16);
		te[i]._block = 0;
		te[i]._usage_duration = random() % 200;
		te[i]._alloc_time = 0;

		req_total_size += te[i]._size;
	}

	printf("req_total_size : %ld\n", req_total_size);
	running = 1;

	struct sigaction sa;
	sa.sa_sigaction = signal_stop;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &sa, 0);

	mmp_freelist_profile(mp);

	while(running)
	{
		gettimeofday(&tv, 0);
		now_time = tv.tv_sec * 1000000 + tv.tv_usec;
		loop_count++;

		for(long i = 0; i < node_count; ++i)
		{
			if(enable_alloc)
			{
				if(te[i]._alloc_time == 0)
				{
					te[i]._block = mmp_alloc(mp, te[i]._size);
					if(!te[i]._block)
					{
						printf("alloc error, loopcount: %ld, idx: %ld, reqsize: %ld.\n", loop_count, i, te[i]._size);
						enable_alloc = 0;
						restart_alloc_time = now_time + 2000;
					}
					else
					{
						te[i]._alloc_time = now_time;

						printf("--- alloc [%ld], idx: %ld, duration: %ld.\n", te[i]._size, i, te[i]._usage_duration);

//						mmp_freelist_profile(mp);
					}
				}
			}
			else if(restart_alloc_time > now_time)
			{
				enable_alloc = 1;
				restart_alloc_time = 0;
			}


			if(te[i]._alloc_time + te[i]._usage_duration < now_time)
			{
				rslt = mmp_free(mp, te[i]._block);
				if(rslt < 0)
				{
					printf("free error, loopcount: %ld, idx: %ld.\n", loop_count, i);
					goto error_ret;
				}

				printf("--- free [%ld], idx: %ld.\n", te[i]._size, i);
				te[i]._alloc_time = 0;
			}

//			mmp_freelist_profile(mp);

		}
		if(rslt < 0) goto error_ret;
loop_continue:
		usleep(10);
	}

	mmp_del(mp);

	printf("test_mmp successed.\n");
	return 0;
error_ret:
	mmp_check(mp);
	printf("test_mmp failed.\n");
	return -1;
}

long profile_mmpool(void)
{
	long rslt = 0;
	unsigned int size = 100 * 1024 * 1024;
	long rnd = 0;
	unsigned long r1 = 0, r2 = 0;

	unsigned long tmp = 0;
	unsigned long alloc_sum = 0, free_sum = 0;
	unsigned long alloc_max = 0, free_max = 0;
	unsigned long count = 1000;

	mmp_buf = malloc(size);
	struct mmpool_config cfg;
	cfg.min_block_index = 6;
	cfg.max_block_index = 10;

	struct mmpool* pool = mmp_new(mmp_buf, size, &cfg);

	if(!pool) goto error_ret;

	for(long i = 0; i < count; i++)
	{
		rnd = random() % 1024;

		if(rnd <= 0)
			continue;

		r1 = rdtsc();
		void* p = mmp_alloc(pool, rnd);
		r2 = rdtsc();

		tmp = r2 - r1;
		alloc_sum += tmp;
		if(tmp > alloc_max)
			alloc_max = tmp;


		if(!p) printf("alloc errrrrrrrrrrrrrrrrror.\n");

		r1 = rdtsc();
		mmp_free(pool, p);
		r2 = rdtsc();

		tmp = r2 - r1;
		free_sum += tmp;
		if(tmp > free_max)
			free_max = tmp;
	}

	rslt = mmp_check(pool);

	printf("[avg] alloc cycle: %lu, free cycle: %lu.\n", alloc_sum / count, free_sum / count);
	printf("[max] alloc cycle: %lu, free cycle: %lu.\n", alloc_max, free_max);

	mmp_del(pool);

	return 0;
error_ret:
	return -1;
}

void test_mmpool_ex(void)
{

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

//unsigned long test_asm(unsigned long val)
//{
//	unsigned long k = 100;
//
//	__asm__("addq %1, %0\n\t" : "+r"(k) : ""(val));
//
//	return k;
//}
//
//unsigned long test_asm_align8(unsigned long val)
//{
//	unsigned long ret;
//	__asm__("addq $8, %1\n\t"
//			"andq $-8, %1\n\t"
//			"movq %1, %0\n\t"
//			: "=r"(ret), "+r"(val));
//
//	return ret;
//}

long test_mmcpy(void)
{
	unsigned long r1 = 0, r2 = 0;
	long rslt = 0;
	long len = random() % 10000000 + 10000000;

	static const char* alpha_beta
		= "abcdefghijlmnopqrstuvwxyz1234567890";

	char* src = malloc(len);
	char* dst = malloc(len);

	printf("test string len: %ld.\n", len);

	for(long i = 0; i < len; ++i)
	{
		src[i] = alpha_beta[random() % strlen(alpha_beta)];
	}
	src[len - 1] = 0;

	r1 = rdtsc();
	rslt = memcpy(dst, src, len);
	r2 = rdtsc();
	printf("memcpy cycle: %lu.\n", r2 - r1);

	r1 = rdtsc();
	rslt = quick_mmcpy(dst, src, len);
	r2 = rdtsc();

//	printf("src: [%s]\n", src);
//	printf("dst: [%s]\n", dst);

	for(long i = 0; i < len; ++i)
	{
		if(dst[i] != src[i])
		{
			printf("mmcpy error @%ld.\n", i);
			break;
		}
	}

	printf("quick_mmcpy cycle: %lu.\n", r2 - r1);


	free(dst);
	free(src);
	return rslt;
}


long test_qqq(long a, long b, long c)
{
	if((a & 0xf) != 0 || (b & 0xf) != 0 || (c & 0xf) != 0)
		return -1;

	a += 1;
	b += 2;
	c += 3;

	return a + b + c;
}

int main(void)
{
	unsigned long i = test_asm_align8(10);
	printf("%lu\n", i);

	srandom(time(0));

	test_mmcpy();
//	test_mmp(1024 * 1024, 6, 10, 64);
//	profile_mmpool();



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
