//------------------------------------------------------------------------------
// Double-linked list
//------------------------------------------------------------------------------

#pragma once


struct list_node_s
	{
	struct list_node_s * prev;
	struct list_node_s * next;
	};

typedef struct list_node_s list_node_t;


void list_init (list_node_t * root);

void insert_before (list_node_t * next, list_node_t * node);
void insert_after (list_node_t * prev, list_node_t * node);

void list_add_tail (list_node_t * root, list_node_t * node);
void list_add_head (list_node_t * root, list_node_t * node);

void list_remove (list_node_t * node);


//------------------------------------------------------------------------------
