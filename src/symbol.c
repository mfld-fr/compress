//------------------------------------------------------------------------------
// Symbols
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <math.h>

#include "list.h"
#include "stream.h"
#include "symbol.h"


// Global data

list_t sym_root;
uint_t sym_count;

list_t pos_root;
uint_t pos_count;

index_sym_t index_sym [SYMBOL_MAX];
uint_t index_count;


// Local data

static list_t pair_root;
static list_t hole_root;


// Symbol helpers

static int sym_comp (const void * v1, const void * v2)
	{
	uint_t k1 = ((index_sym_t *) v1)->key;
	uint_t k2 = ((index_sym_t *) v2)->key;

	return (k1 < k2) ? 1 : ((k1 > k2) ? -1 : 0);
	}


symbol_t * sym_add ()
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * sym = malloc (sizeof (symbol_t));
	memset (sym, 0, sizeof (symbol_t));

	list_add_tail (&sym_root, (list_t *) sym);  // node as first member
	sym_count++;

	return sym;
	}


// Build index and sort
// TODO: build key in callback

uint_t sym_sort (uint_t filter)
	{
	uint_t filt_count = 0;

	list_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = (symbol_t *) node;  // node as first member

		uint_t dup_count = sym->pos_count + sym->sym_count;  // number of duplicates

		switch (filter)
			{
			case SORT_ALL:
				index->key = (dup_count + ((sym->rep_count > 1) ? sym->rep_count : 0)) * sym->size;
				filt_count++;
				break;

			case SORT_REP:
				if (sym->rep_count > 1)
					{
					index->key = 0;
					break;
					}

				index->key = dup_count;
				filt_count++;
				break;

			case SORT_DUP:
				if (sym->size == 1 || (dup_count == 1 && sym->rep_count != 1))
					{
					index->key = 0;
					break;
					}

				index->key = dup_count;
				filt_count++;
				break;

			}

		index->sym = sym;

		node = node->next;
		}

	qsort (index_sym, sym_count, sizeof (index_sym_t), sym_comp);

	// Set symbol indexes

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		sym->index = i;
		}

	return filt_count;
	}


// List the used symbols

void sym_list ()
	{
	double entropy = 0.0;

	puts ("SYMBOLS");

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		uint_t sym_dup = sym->pos_count + sym->sym_count;

		double p = (double) sym_dup / (pos_count + sym_count);
		entropy += -p * log2 (p);

		printf ("[%u] base=%x", i, sym->base);

		if (sym->size == 1)
			printf (" code=%hx", sym->code);
		else
			printf (" size=%u", sym->size);

		if (sym->rep_count > 1)
			printf (" rep=%u", sym->rep_count);
		else
			printf (" pos=%u", sym->pos_count);

		printf (" sym=%u\n", sym->sym_count);
		}

	printf ("\nentropy=%f\n\n", entropy);
	}


static void hole_add (position_t * pos)
	{
	hole_t * hole = malloc (sizeof (hole_t));
	hole->pos = pos;
	list_add_tail (&hole_root, (list_t *) hole);  // node as first member
	}


// Scan frame for all base symbols

void scan_base ()
	{
	list_init (&sym_root);
	list_init (&pos_root);

	list_init (&hole_root);

	memset (index_sym, 0, sizeof (index_sym_t) * CODE_MAX);

	// Count symbol occurrences

	for (uint_t i = 0; i < size_in; i++)
		{
		index_sym_t * index = index_sym + frame_in [i];
		symbol_t * sym = index->sym;

		if (!sym)
			{
			sym = sym_add ();

			sym->code = frame_in [i];
			sym->base = i;
			sym->size = 1;

			index->sym = sym;
			}

		position_t * pos = malloc (sizeof (position_t));
		list_add_tail (&pos_root, (list_t *) pos);  // node as first member

		pos->base = i;
		pos->sym = sym;
		pos->pair = NULL;

		// Record hole (= position without pair)

		if (i + 1 < size_in) hole_add (pos);

		sym->pos_count++;
		}

	pos_count = size_in;
	}


// Scan frame holes for new pairs

static void scan_pair ()
	{
	list_t * hole_left = hole_root.next;
	while (hole_left != &hole_root)
		{
		position_t * pos_left = ((hole_t *) hole_left)->pos;  // node as first member
		position_t * pos_left_next = (position_t *) (pos_left->node.next);

		// Add new pair

		pair_t * pair = malloc (sizeof (pair_t));
		list_add_tail (&pair_root, (list_t *) pair);  // node as first member

		pair->count = 1;

		pair->left = pos_left->sym;
		pair->right = pos_left_next->sym;

		pos_left->pair = pair;  // pair now found there

		// Scan for pair duplicates

		list_t * hole_right = hole_left->next;
		while (hole_right != &hole_root)
			{
			list_t * hole_right_next = hole_right->next;

			position_t * pos_right = ((hole_t *) hole_right)->pos;  // node as first member
			position_t * pos_right_next = (position_t *) (pos_right->node.next);

			if ((pair->left == pos_right->sym) &&
				(pair->right == pos_right_next->sym))
				{
				pair->count++;

				pos_right->pair = pair;  // pair now found there
				list_remove (hole_right);
				memset (hole_right, 0, sizeof (hole_t));  // invalidate pointers
				free ((hole_t *) hole_right);  // node as first member
				}

			hole_right = hole_right_next;
			}

		list_t * hole_left_next = hole_left->next;
		list_remove (hole_left);
		memset (hole_left, 0, sizeof (hole_t));  // invalidate pointers
		free ((hole_t *) hole_left);  // node as first member
		hole_left = hole_left_next;
		}
	}


static void dec_pair (pair_t * pair)
	{
	pair->count--;

	if (!pair->count)
		{
		list_remove ((list_t *) pair);  // node as first member
		memset (pair, 0, sizeof (pair_t));  // invalidate pointers
		free (pair);
		}
	}


// Crunch all occurrences of one pair

static int crunch_pair (pair_t * pair)
	{
	int shrink = 0;  // no shrink

	// Check all pair occurrences

	symbol_t * sym = NULL;

	list_t * node = pos_root.next;
	list_t * node_prev = &pos_root;

	while ((node != pos_root.prev) && (node != &pos_root))
		{
		list_t * node_next = node->next;

		position_t * pos = (position_t *) node;  // node as first member
		if (pos->pair == pair)
			{
			// Consider previous pair

			if (node_prev != &pos_root)
				{
				position_t * pos_prev = (position_t *) node_prev;  // node as first member
				if (pos_prev->pair)  // can be the pair just processed before
					{
					dec_pair (pos_prev->pair);
					pos_prev->pair = NULL;
					hole_add (pos_prev);
					}
				}

			// Consider next pair

			if ((node_next != pos_root.prev) && (node_next != &pos_root))
				{
				position_t * pos_next = (position_t *) node_next;  // node as first member
				dec_pair (pos_next->pair);
				pos_next->pair = NULL;
				//hole_add (pos_next);  // position will be crunched later
				}

			// Replace current pair by new symbol

			symbol_t * sym_left = pair->left;
			symbol_t * sym_right = pair->right;

			if (!sym)
				{
				sym = sym_add ();

				sym->base = pos->base;
				sym->size = sym_left->size + sym_right->size;

				sym->left = sym_left;
				sym_left->sym_count++;

				sym->right = sym_right;
				sym_right->sym_count++;
				}

			sym_left->pos_count--;
			sym_right->pos_count--;
			sym->pos_count++;

			pos->sym = sym;

			dec_pair (pair);
			pos->pair = NULL;
			hole_add (pos);

			// Shift frame end to left

			list_t * node_after = node_next->next;
			list_remove (node_next);
			free ((position_t *) node_next);  // node as first member
			node_next = node_after;

			pos_count--;

			shrink = 1;
			}

		node_prev = node;
		node = node_next;
		}

	return shrink;
	}


// Crunch all pairs
// Performed alone or before repeat crunch

void crunch_word ()
	{
	// Iterate on pair scan & crunch

	list_init (&pair_root);

	while (1)
		{
		scan_pair ();

		// Look for the pair the most duplicated

		uint_t count_max = 0;
		pair_t * pair_max = NULL;

		list_t * node = pair_root.next;
		while (node != &pair_root)
			{
			pair_t * pair = (pair_t *) node;  // node as first member

			// Skip any symmetric pair

			if (pair->left != pair->right)
				{
				uint_t count = pair->count;
				if (count > count_max)
					{
					count_max = count;
					pair_max = pair;
					}
				}

			node = node->next;
			}

		if (count_max < 2) break;

		if (!crunch_pair (pair_max)) break;
		}
	}


// Crunch all repeated symbols
// Performed alone or after word crunch

void crunch_rep ()
	{
	list_t * node_left = pos_root.next;
	while ((node_left != pos_root.prev) && (node_left != &pos_root))
		{
		position_t * pos_left = (position_t *) node_left;  // node as first member

		symbol_t * sym_left = pos_left->sym;
		symbol_t * sym_rep = NULL;

		list_t * node_right = node_left->next;
		while (1)
			{
			if (node_right == &pos_root) break;

			position_t * pos_right = (position_t *) node_right;  // node as first member
			if (sym_left != pos_right->sym) break;

			// Replace current symbol by repeat

			if (!sym_rep)
				{
				dec_pair (pos_left->pair);
				pos_left->pair = NULL;

				sym_rep = sym_add ();

				pos_left->sym = sym_rep;
				sym_rep->pos_count = 1;
				sym_left->pos_count--;

				sym_rep->rep_count = 1;
				sym_left->rep_count = 1;  // repeated

				sym_rep->base = pos_left->base;
				sym_rep->size = sym_left->size;

				sym_rep->left = sym_left;
				sym_left->sym_count++;
				}

			sym_left->pos_count--;

			sym_rep->rep_count++;

			dec_pair (pos_right->pair);
			pos_right->pair = NULL;
			pos_right->sym = NULL;

			// Shift frame end to left

			list_t * node_after = node_right->next;
			list_remove (node_right);
			free ((position_t *) node_right);  // node as first member
			node_right = node_after;

			pos_count--;
			}

		node_left = node_right;
		}
	}


//------------------------------------------------------------------------------
