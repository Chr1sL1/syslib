#include <stdio.h>
#include "common.h"
#include "dlist.h"
#include "rbtree.h"
#include "graph.h"

int graph_new(struct graph* g)
{
	if(!g) goto error_ret;

	g->gnode_table.size = 0;
	g->gnode_table.root = NULL;

	return 1;
error_ret:
	return 0;
}

int graph_add_node(struct graph* g, struct gnode* node, int key)
{
	if(!g || !node) goto error_ret;

	rb_fillnew(&node->table_node);
	lst_new(&node->ngb_list);

	node->table_node.key = key;
	node->visited = 0;

	if(!rb_insert(&g->gnode_table, &node->table_node))
		goto error_ret;

	return 1;
error_ret:
	return 0;
}

struct gnode* graph_del_node(struct graph* g, int node_key)
{
	struct rbnode* rbn = NULL;
	struct gnode* ret = NULL;

	if(!g) goto error_ret;

	rbn = rb_search(&g->gnode_table, node_key, NULL);
	if(!rbn) goto error_ret;

	ret = node_cast(gnode, rbn, table_node);

	return ret;
error_ret:
	return NULL;
}

int graph_add_edge(struct graph* g, struct ngb_node* ngb1, struct ngb_node* ngb2)
{
	struct rbnode* rbn1 = NULL;
	struct rbnode* rbn2 = NULL;

	struct gnode* nd1 = NULL;
	struct gnode* nd2 = NULL;

	if(!g) goto error_ret;

	rbn1 = rb_search(&g->gnode_table, ngb1->key, NULL);
	if(!rbn1) goto error_ret;

	rbn2 = rb_search(&g->gnode_table, ngb2->key, NULL);
	if(!rbn2) goto error_ret;

	nd1 = node_cast(gnode, rbn1, table_node);
	nd2 = node_cast(gnode, rbn2, table_node);

	lst_clr(&ngb1->ngb_node);
	lst_clr(&ngb2->ngb_node);

	lst_insert_before(&nd1->ngb_list, &nd1->ngb_list.tail, &ngb2->ngb_node);
	lst_insert_before(&nd2->ngb_list, &nd2->ngb_list.tail, &ngb1->ngb_node);

	return 1;
error_ret:
	return 0;
}

int graph_bfs(struct graph* g, int node_from, _search_func f)
{
	return 1;
error_ret:
	return 0;
}

int graph_dfs(struct graph* g, int node_from, _search_func f)
{
	return 1;
error_ret:
	return 0;
}


