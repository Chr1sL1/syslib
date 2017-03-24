#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "shmem.h"
#include "rbtree.h"

const char* share_memory_name = "test_shm_17x";

struct list_head
{
	struct list_head* prev;
	struct list_head* post;
};

struct ad_test
{
	int _value1;
	int _value2;
	struct list_head _head;
};

void test_link(void)
{
	struct ad_test __test;
	printf("_head addr: %p\n", &__test._head);

	__test._value1 = 746;
	__test._value2 = 626;

	struct ad_test* p = (struct ad_test*)((void*)&__test._head - (void*)&((struct ad_test*)0)->_head);

	printf("off head addr: %p\n", &((struct ad_test*)0)->_head);
	printf("actual addr: %p\n", &__test);
	printf("p addr: %p\n", p);
	printf("%d,%d\n", p->_value1, p->_value2);

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

//	int test_arr[] = { 3,2,5,4,6,8,7,9,1,0,11,12,13,14,15,16,17,18,19,10 };
	int test_arr[] = { 1,2,3,4,5,6,7,8,9,10 };

	struct rbtree test_tree;
	test_tree.size = 0;
	test_tree.root = NULL;

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
	{
		struct rbnode* node = (struct rbnode*)malloc(sizeof(struct rbnode));
		rb_fillnew(&test_tree, node);

		node->key = test_arr[i];

		rb_insert(&test_tree, node);

		pre_order(test_tree.root, print_node);
		printf("\n");
		in_order(test_tree.root, print_node);
		printf("\n---size %d---\n", test_tree.size);
	}

	printf("rooooooooooooooooooot:%d\n", test_tree.root->key);

	for(int i = 0; i < sizeof(test_arr) / sizeof(int); i++)
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



	return 0;
}
