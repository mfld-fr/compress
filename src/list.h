//------------------------------------------------------------------------------
// Double-linked list
//------------------------------------------------------------------------------

#pragma once


struct list_s
	{
	struct list_s * prev;
	struct list_s * next;
	};

typedef struct list_s list_t;


void list_init (list_t * root);

void insert_before (list_t * next, list_t * node);
void insert_after (list_t * prev, list_t * node);

void list_add_tail (list_t * root, list_t * node);
void list_add_head (list_t * root, list_t * node);

void list_remove (list_t * node);


//------------------------------------------------------------------------------
