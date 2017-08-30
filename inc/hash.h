#ifndef __hash_h__
#define __hash_h__

#define HASH_KEY_LEN (64)

struct hash_node
{
	char hash_key[HASH_KEY_LEN];
	void* u_data;
};

typedef struct hash_node* (*hash_node_alloc_func)(void);

struct hash_table
{
	hash_node_alloc_func alloc_func;

};

#endif

