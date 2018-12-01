//------------------------------------------------------------------------------
// Double-linked list
//------------------------------------------------------------------------------

#include "list.h"


void list_init (list_t * root)
	{
	root->prev = root;
	root->next = root;
	}


#define LIST_LINK \
	prev->next = node; \
	node->prev = prev; \
	next->prev = node; \
	node->next = next; \
	/**/


void insert_before (list_t * next, list_t * node)
	{
	list_t * prev = next->prev;
	LIST_LINK
	}

void insert_after (list_t * prev, list_t * node)
	{
	list_t * next = prev->next;
	LIST_LINK
	}


void list_add_tail (list_t * root, list_t * node)
	{
	insert_before (root, node);
	}

void list_add_head (list_t * root, list_t * node)
	{
	insert_after (root, node);
	}


void list_remove (list_t * node)
	{
	list_t * prev = node->prev;
	list_t * next = node->next;
	prev->next = next;
	next->prev = prev;
	}


//------------------------------------------------------------------------------
