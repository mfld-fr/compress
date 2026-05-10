//------------------------------------------------------------------------------
// Symbols
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <math.h>

#include "common.h"
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

list_t levels [LEVEL_MAX];
uint_t level_count;

uint keep_count;


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


symbol_t * sym_add (uint level)
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * sym = malloc (sizeof (symbol_t));
	memset (sym, 0, sizeof (symbol_t));

	list_add_tail (&sym_root, (list_t *) sym);  // node as first member
	sym_count++;

	// Add symbol to level list

	if (level >= LEVEL_MAX)
		error (1, 0, "too many levels");

	list_add_tail (&levels [level], &sym->node_level);

	sym->level = level;

	if (level + 1 > level_count)
		level_count = level + 1;

	return sym;
	}


// Build index and sort
// TODO: build key in callback

uint_t sym_sort (uint_t kind)
	{
	list_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = (symbol_t *) node;  // node as first member

		sym->dup_count = sym->sym_count + sym->pos_count;

		switch (kind)
			{
			case SORT_ALL:
				index->key = sym->dup_count + (sym->rep_count > 1 ? sym->rep_count : 0) * sym->size;
				break;

			case SORT_REP:
				if (sym->rep_count > 1)
					{
					index->key = 0;
					break;
					}

				index->key = sym->dup_count;
				break;

			case SORT_DUP:
				index->key = sym->dup_count;
				break;

			default:
				error (1, 0, "unknown sorting");
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

	return 0;
	}


// Initial symbol filtering

uint_t filter_init ()
	{
	uint_t filt_count = 0;

	list_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		symbol_t * sym = (symbol_t *) node;  // node as first member

		sym->dup_count = sym->pos_count + sym->sym_count;
		if (sym->dup_count > 1)
			{
			// Duplicated symbols are presumed valuable
			// until cost computation confirms or not
			sym->keep = 1;
			filt_count++;
			}

		node = node->next;
		}

	return filt_count;
	}


// List the used symbols

void sym_list (uint_t filter)
	{
	double entropy = 0.0;
	uint use_count = 0;

	puts ("SYMBOLS:");

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		if (filter == LIST_KEEP && !sym->keep) continue;

		uint_t sym_dup = sym->pos_count + sym->sym_count;  // TODO: replace by sym->dup_count
		use_count += sym_dup;

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

		printf (" tree=%u\n", sym->sym_count);
		}

	printf ("\nEntropy: %f\n", entropy);
	printf ("Core size: %u\n\n", (uint) (entropy * use_count / 8));
	}


// Add a position to the hole list

static void hole_add (position_t * pos)
	{
	list_add_tail (&hole_root, &pos->node_hole);
	}

// Remove a position from the hole list

static void hole_remove (position_t * pos)
	{
	list_remove (&pos->node_hole);
	// Zero the node_hole to detect bad pointers
	memset (&pos->node_hole, 0, sizeof (list_t));
	}


// Scan frame for all base symbols

void scan_base ()
	{
	list_init (&sym_root);
	list_init (&pos_root);
	list_init (&hole_root);

	// Initialize the level array

	for (int i = 0; i < LEVEL_MAX; i++)
		list_init (&levels [i]);

	// Initialize the symbol index

	memset (index_sym, 0, sizeof (index_sym_t) * CODE_MAX);

	// Count symbol occurrences

	for (uint_t i = 0; i < size_in; i++)
		{
		index_sym_t * index = index_sym + frame_in [i];
		symbol_t * sym = index->sym;

		if (!sym)
			{
			sym = sym_add (0);  // 0 for base level

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
		position_t * pos_left = structof (position_t, node_hole, hole_left);
		position_t * pos_left_next = (position_t *) (pos_left->node.next);  // node as first member

		// Replace hole by a new pair

		pair_t * pair = malloc (sizeof (pair_t));
		list_add_tail (&pair_root, (list_t *) pair);  // node as first member

		pair->count = 1;

		pair->left = pos_left->sym;
		pair->right = pos_left_next->sym;

		pos_left->pair = pair;  // pair at position now

		// Scan for pair duplicates

		list_t * hole_right = hole_left->next;
		while (hole_right != &hole_root)
			{
			list_t * hole_right_next = hole_right->next;

			position_t * pos_right = structof (position_t, node_hole, hole_right);
			position_t * pos_right_next = (position_t *) (pos_right->node.next);  // node as first member

			if ((pair->left == pos_right->sym) &&
				(pair->right == pos_right_next->sym))
				{
				pair->count++;

				pos_right->pair = pair;  // pair at position now
				hole_remove (pos_right);
				}

			hole_right = hole_right_next;  // TODO: simplify
			}

		hole_left = hole_left->next;
		hole_remove (pos_left);
		}
	}


static void dec_pair (pair_t * pair)
	{
	pair->count--;

	if (!pair->count)
		{
		list_remove ((list_t *) pair);  // node as first member
		// Zero the pair to detect bad pointers
		memset (pair, 0, sizeof (pair_t));
		free (pair);
		}
	}


// Crunch all occurrences of one pair

static int crunch_pair (pair_t * pair)
	{
	int shrink = 0;  // no shrink

	// Check all pair occurrences

	symbol_t * sym = NULL;

	list_t * node_prev = &pos_root;
	list_t * node_left = pos_root.next;
	list_t * node_right = node_left->next;
	list_t * node_next = node_right->next;

	while (1)
		{
		position_t * pos_left = (position_t *) node_left;  // node as first member
		if (pos_left->pair == pair)
			{
			// Consider previous pair if any

			if (node_prev != &pos_root)
				{
				position_t * pos_prev = (position_t *) node_prev;  // node as first member
				if (pos_prev->pair)
					{
					dec_pair (pos_prev->pair);
					pos_prev->pair = NULL;
					hole_add (pos_prev);
					}
				}

			// Consider next pair if any

			if (node_next != &pos_root)
				{
				position_t * pos_right = (position_t *) node_right;  // node as first member
				if (pos_right->pair)
					{
					dec_pair (pos_right->pair);
					pos_right->pair = NULL;   // position will be crunched later
					// hole_add (pos_right);  // position will be crunched later
					}
				}

			// Replace current pair by new symbol

			symbol_t * sym_left = pair->left;
			symbol_t * sym_right = pair->right;

			if (!sym)
				{
				uint level = 1 + (sym_left->level > sym_right->level ? sym_left->level : sym_right->level);

				sym = sym_add (level);

				sym->base = pos_left->base;
				sym->size = sym_left->size + sym_right->size;

				sym->left = sym_left;
				sym_left->sym_count++;

				sym->right = sym_right;
				sym_right->sym_count++;
				}

			sym_left->pos_count--;
			sym_right->pos_count--;
			sym->pos_count++;

			pos_left->sym = sym;

			dec_pair (pair);
			pos_left->pair = NULL;

			if (node_next != &pos_root)
				hole_add (pos_left);

			// Shift end of the frame to the left

			list_remove (node_right);
			free ((position_t *) node_right);  // node as first member
			node_right = NULL;
			pos_count--;
			shrink = 1;

			if (node_next == &pos_root) break;
			node_prev = node_left;
			node_left = node_next;
			node_right = node_left->next;
			if (node_right == &pos_root) break;
			node_next = node_right->next;
			}

		else
			{
			if (node_next == &pos_root) break;
			list_t * node = node_next->next;
			node_prev = node_left;
			node_left = node_right;
			node_right = node_next;
			node_next = node;
			}
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
				if (pair->count > count_max)
					{
					count_max = pair->count;
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

				uint level = 1 + sym_left->level;
				sym_rep = sym_add (level);

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


// Walk the symbol tree to compute symbol usage order

uint sym_order (symbol_t * sym, uint order)
	{
	if (sym->order) return order;  // skip if already set earlier

	sym->order = order++;
	if (sym->level == 0) return order;

	if (sym->left) order = sym_order (sym->left, order);
	if (sym->right) order = sym_order (sym->right, order);
	return order;
	}


// Compute symbol cost and decide whether to keep or drop it

void sym_cost (symbol_t * sym, uint bit_len)
	{
	uint cost_use;
	uint cost_def;
	uint cost_ref = 2 + bit_len;  // 2 bits for reference prefix '11'

	if (sym->level == 0)
		{
		// Base symbol

		sym->len = 1;
		cost_use = 1 + 8;
		cost_def =  2 + cost_use;
		}
	else
		{
		// Derived symbol

		sym->len = sym->left->keep ? 1 : sym->left->len;
		sym->len += sym->right->keep ? 1 : sym->right->len;

		uint cost = (sym->left->order > sym->order) ? sym->left->cost_first : sym->left->cost_next;
		cost += (sym->right->order > sym->order) ? sym->right->cost_first : sym->right->cost_next;

		cost_use = cost;
		cost_def = 2 + cost_pref_odd (sym->len - 1) + cost;
		}

	// Consider both costs whenever kept or not

	uint cost_drop = sym->dup_count * cost_use;
	uint cost_keep = cost_def + (sym->dup_count -1) * cost_ref;
	int cost_gain = cost_drop - cost_keep;

	// Keep or drop the symbol according to the cost gain

	if (cost_gain <= 0)
		{
		sym->keep = 0;
		sym->cost_first = cost_use;
		sym->cost_next = cost_use;

		// FIXME: dropping a derived symbol causes more children usage
		// so have to update the duplicate count of the children first
		// then to restart cost computation & selection one level down
		if ((sym->level) > 0 && (sym->dup_count > 1))
			{
			// printf ("Missing code at %s(%u)\n", __FILE__, __LINE__);
			}
		}
	else
		{
		sym->keep = 1;
		sym->cost_first = cost_def;
		sym->cost_next = cost_ref;
		keep_count++;
		}
	}

//------------------------------------------------------------------------------

