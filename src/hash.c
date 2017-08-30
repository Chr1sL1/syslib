#include "hash.h"
#include <string.h>

unsigned long _hash_value(const char* key, unsigned long bucket_size)
{
	unsigned long sum = 0;

	for(int i = 0; key[i] != 0; ++i)
		sum += key[i];

	return sum % bucket_size;
}


inline long hash_insert(struct hash_table* ht, struct hash_node* hn)
{
	unsigned long hash_value;

	if(!ht || !hn) goto error_ret;
	if(ht->bucket_size == 0) goto error_ret;

	lst_clr(&hn->list_node);

	hash_value = _hash_value(hn->hash_key, ht->bucket_size);

	return lst_push_back(&ht->hash_list[hash_value], &hn->list_node);
error_ret:
	return -1;
}

struct hash_node* hash_search(struct hash_table* ht, const char* key)
{
	struct dlnode* dln;
	struct hash_node* hn = 0;
	unsigned long hash_value;

	if(!ht || !key) goto error_ret;
	if(ht->bucket_size == 0) goto error_ret;

	hash_value = _hash_value(key, ht->bucket_size);

	dln = ht->hash_list[hash_value].head.next;

	while(dln != &ht->hash_list[hash_value].tail)
	{
		hn = (struct hash_node*)((unsigned long)dln - (unsigned long)(&((struct hash_node*)(0))->list_node));

		if(strcmp(hn->hash_key, key) == 0)
			break;

		dln = dln->next;
	}

	return hn;
error_ret:
	return 0;
}
