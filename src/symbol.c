//------------------------------------------------------------------------------
// Symbols
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <math.h>
#include <limits.h>

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

uint keep_count;
uchar ref_bit;


// Local data

static list_t pair_root;
static list_t hole_root;


// Symbol helpers

static int sym_comp (const void * v1, const void * v2)
	{
	int k1 = ((index_sym_t *) v1)->key;
	int k2 = ((index_sym_t *) v2)->key;

	return (k1 < k2) ? 1 : ((k1 > k2) ? -1 : 0);
	}


symbol_t * sym_add ()
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * sym = malloc (sizeof (symbol_t));
	memset (sym, 0, sizeof (symbol_t));

	list_add_tail (&sym_root, &sym->node);
	sym_count++;
	return sym;
	}

// Build index and sort
// TODO: build key in callback

void sym_sort (uint_t kind)
	{
	list_t * node = sym_root.next;
	index_sym_t * index = index_sym;

	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);

		sym->use_count = sym->pos_count + sym->sym_count + sym->rep_pos;

		switch (kind)
			{
			case SORT_ALL:
				if (sym->repeat)
					index->key = sym->rep_count * sym->size;
				else
					index->key = sym->use_count * sym->size;
				break;

			case SORT_REP:
				if (sym->repeat)
					{
					index->key = 0;
					break;
					}

				index->key = sym->use_count;
				break;

			case SORT_USE:
				index->key = sym->use_count;
				break;

			case SORT_GAIN:
				index->key = sym->repeat ? INT_MIN : sym->gain;
				break;

			case SORT_BASE:
				index->key = sym->repeat ? INT_MIN : -((sym->base << 4) + sym->size);
				break;

			case SORT_SIZE:
				index->key = sym->repeat ? INT_MIN : -sym->size;
				break;

			default:
				error (1, 0, "unknown sorting");
			}

		index->sym = sym;

		node = node->next;
		index++;
		}

	qsort (index_sym, sym_count, sizeof (index_sym_t), sym_comp);

	// Set symbol indexes

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		sym->index = i;
		}
	}


// List the used symbols

void sym_list (uint_t filter)
	{
	double entropy = 0.0;

	puts ("SYMBOLS:");

	uint keep_index = 0;
	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		if (filter == LIST_KEEP && !sym->keep) continue;

		if (!sym->repeat)
			{
			// FIXME: global counts distorted by repeat symbols
			double p = (double) sym->use_count / (pos_count + sym_count);
			entropy += -p * log2 (p);
			}

		printf ("[%u]", (filter == LIST_KEEP) ? keep_index : i);
		printf (" base=%x", sym->base);

		if (sym->size == 1)
			printf (" code=%hx", sym->code);
		else
			printf (" size=%u", sym->size);

		if (!sym->repeat)
			{
			printf (" pos=%u", sym->pos_count);
			printf (" tree=%u", sym->sym_count);
			printf (" rpos=%u", sym->rep_pos);
			}

		printf (" use=%u", sym->use_count);
		printf (" rep=%u", sym->rep_count);

		if (sym->cost)
			printf (" gain=%i", sym->gain);

		puts ("");

		keep_index++;
		}

	printf ("\nEntropy: %f\n\n", entropy);
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

	// Initialize the symbol index

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
		list_add_tail (&pos_root, &pos->node);

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
		list_add_tail (&pair_root, &pair->node);

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
					// position will be crunched later
					// pos_right->pair = NULL;
					// hole_add (pos_right);
					}
				}

			// Replace current pair by new symbol

			symbol_t * sym_left = pair->left;
			symbol_t * sym_right = pair->right;

			if (!sym)
				{
				sym = sym_add ();

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

			// Shift frame end to the left

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
		position_t * pos_left = structof (position_t, node, node_left);

		symbol_t * sym_left = pos_left->sym;
		symbol_t * sym_rep = NULL;

		list_t * node_right = node_left->next;
		while (node_right != &pos_root)
			{
			position_t * pos_right = structof (position_t, node, node_right);
			symbol_t * sym_right = pos_right->sym;
			if (sym_left != sym_right) break;

			// Replace current symbol by repeat

			if (!sym_rep)
				{
				if (pos_left->pair) dec_pair (pos_left->pair);
				pos_left->pair = NULL;

				sym_rep = sym_add ();
				sym_rep->repeat = 1;

				pos_left->sym = sym_rep;
				sym_rep->pos_count = 1;
				sym_left->pos_count--;
				sym_left->rep_pos++;

				sym_rep->rep_count = 1;
				sym_left->rep_count++;

				sym_rep->code = sym_left->code;
				sym_rep->base = sym_left->base;
				sym_rep->size = sym_left->size;

				sym_rep->left = sym_left;
				}

			sym_right->pos_count--;

			sym_rep->rep_count++;
			sym_right->rep_count++;

			if (pos_right->pair) dec_pair (pos_right->pair);
			// position will be crunched later
			// pos_right->pair = NULL;
			// pos_right->sym = NULL;

			// Shift frame end to the left

			list_t * node_next = node_right->next;
			list_remove (node_right);
			free (pos_right);
			node_right = node_next;

			pos_count--;
			}

		node_left = node_right;
		}
	}


// Keep duplicated symbols

uint_t keep_dup ()
	{
	uint_t count = 0;

	list_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		symbol_t * sym = structof (symbol_t, node, node);

		sym->use_count = sym->pos_count + sym->sym_count + sym->rep_pos;
		if (sym->use_count > 1 || (!sym->repeat && sym->rep_count > 1))
			{
			// Duplicated or repeated symbols are presumed valuable
			// until cost computation confirms or not
			sym->keep = 1;
			count++;
			}
		else
			{
			sym->keep = 0;
			}

		node = node->next;
		}

	return count;
	}


// Compute symbol cost in SE algorithm
// Decide whether to define it (keep) or not (drop)

uint sym_cost_se (symbol_t * sym, uchar select)
	{
	uint use_cost;
	uint def_cost;
	uint ref_cost = 1 + ref_bit;  // 1 bit for reference prefix '1'

	uint drop_cost;

	if (sym->size == 1)
		{
		// Base symbol

		sym->len = 1;
		use_cost = 1 + 8;  // 1 bit for base prefix '0' and 8 bits for base code
		def_cost = use_cost;
		drop_cost = sym->use_count * use_cost;
		}
	else
		{
		// Derived symbol

		sym->len = (sym->left->keep ? 1 : sym->left->len)
			+ (sym->right->keep ? 1 : sym->right->len);

		use_cost = sym->left->cost + sym->right->cost;
		def_cost = sym->len;  // len = number of next flags
		drop_cost = (sym->use_count - 1) * use_cost;
		}

	uint keep_cost = def_cost + sym->use_count * ref_cost;

	sym->gain = drop_cost - keep_cost;

	// Keep or drop the symbol according to the cost gain

	if (select)
		{
		uchar keep = (sym->gain > 0) ? 1 : 0;
		sym->keep = keep;
		keep_count += keep;
		}

	if (sym->keep)
		{
		sym->cost = ref_cost;
		return keep_cost;
		}

	sym->cost = use_cost;
	return drop_cost;
	}

// Compute symbol cost in SI algorithm
// Decide whether to define it (keep) or not (drop)

uint sym_cost_si (symbol_t * sym, uchar select)
	{
	uint use_cost;
	uint def_cost;
	uint ref_cost = 2 + ref_bit;  // 2 bits for reference prefix '11'

	uint drop_cost;

	if (sym->size == 1)
		{
		// Base symbol

		sym->len = 1;
		use_cost = 1 + 8;  // 1 bit for base prefix
		def_cost =  2 + use_cost;  // 2 bits for definition prefix '10'
		drop_cost = sym->use_count * use_cost;
		}
	else
		{
		// Derived symbol

		sym->len = (sym->left->keep ? 1 : sym->left->len)
			+ (sym->right->keep ? 1 : sym->right->len);

		use_cost = sym->left->cost + sym->right->cost;
		def_cost = 2 + sym->len;  // number of next flags = len
		drop_cost = (sym->use_count - 1) * use_cost;
		}

	uint keep_cost = def_cost + (sym->use_count - 1) * ref_cost;
	sym->gain = drop_cost - keep_cost;

	// Keep or drop the symbol according to the cost gain

	if (select)
		{
		uchar keep = (sym->gain > 0) ? 1 : 0;
		sym->keep = keep;
		keep_count += keep;
		}

	if (sym->keep)
		{
		sym->cost = ref_cost;
		return keep_cost;
		}

	sym->cost = use_cost;
	return drop_cost;
	}


// Compute symbol cost in RSE algorithm
// Decide whether to define it (keep) or not (drop)

uint sym_cost_rse (symbol_t * sym, uchar select)
	{
	uint use_cost;
	uint pos_cost;
	uint def_cost;
	uint drop_cost;

	if (sym->size == 1)
		{
		// Base symbol

		sym->len = 1;
		use_cost = 1 + 8;  // 1 bit for base prefix '0' and 8 bits for base code
		pos_cost = use_cost;
		def_cost = use_cost;
		// A base symbol can always be repeated
		drop_cost = sym->pos_count * pos_cost + (sym->sym_count + sym->rep_pos) * use_cost;
		}
	else
		{
		// Derived symbol

		sym->len = (sym->left->keep ? 1 : sym->left->len)
			+ (sym->right->keep ? 1 : sym->right->len);

		use_cost = sym->left->cost + sym->right->cost;
		pos_cost = sym->left->pcost + sym->right->pcost;

		def_cost = sym->len;  // len = number of next flags
		// A derived symbol cannot be repeated if dropped
		drop_cost = (sym->pos_count + sym->rep_count) * pos_cost + sym->sym_count * use_cost - use_cost;
		}

	// Any symbol can be repeated if kept
	uint keep_cost = def_cost + sym->pos_count * (2 + ref_bit) + (sym->sym_count + sym->rep_pos) * (1 + ref_bit);

	sym->gain = drop_cost - keep_cost;

	// Keep or drop the symbol according to the cost gain

	if (select)
		{
		uchar keep = (sym->gain > 0) ? 1 : 0;
		sym->keep = keep;
		keep_count += keep;
		}

	if (sym->keep)
		{
		sym->cost = 1 + ref_bit;
		sym->pcost = 2 + ref_bit;
		return keep_cost;
		}

	sym->cost = use_cost;
	sym->pcost = pos_cost;
	return drop_cost;
	}

//------------------------------------------------------------------------------

