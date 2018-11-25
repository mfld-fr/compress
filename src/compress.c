//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <error.h>
#include <math.h>
#include <unistd.h>

#include "list.h"
#include "stream.h"


// Symbol definitions

struct symbol_s
	{
	list_node_t node;  // must be the first member

	uint_t pos_count;  // number of occurrences in the frame
	uint_t sym_count;  // number of occurrences in the tree

	uchar_t code;

	uint_t base;  // offset of first occurrence in input frame
	uint_t size;  // size in input codes

	uint_t index;

	// For node symbol (secondary)

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct symbol_s symbol_t;

static list_node_t sym_root;
static uint_t sym_count;


// Pair definitions

struct pair_s;

struct position_s
	{
	list_node_t node;  // must be the first member

	symbol_t * sym;
	struct pair_s * pair;
	};

typedef struct position_s position_t;

static list_node_t pos_root;
static uint_t pos_count;


struct pair_s
	{
	list_node_t node;  // must be the first member

	uint_t count;  // number of occurrences in the frame

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct pair_s pair_t;

static list_node_t pair_root;


// Indexes for quick sort

#define SYMBOL_MAX 65536  // 64K

struct index_sym_s
	{
	uint_t key;
	symbol_t * sym;
	};

typedef struct index_sym_s index_sym_t;

static index_sym_t index_sym [SYMBOL_MAX];


// Program options

uchar_t opt_sym;
uchar_t opt_compress;
uchar_t opt_expand;
uchar_t opt_verb;


static int sym_comp (const void * v1, const void * v2)
	{
	uint_t k1 = ((index_sym_t *) v1)->key;
	uint_t k2 = ((index_sym_t *) v2)->key;

	return (k1 < k2) ? 1 : ((k1 > k2) ? -1 : 0);
	}


static symbol_t * sym_add ()
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * sym = malloc (sizeof (symbol_t));

	sym->pos_count = 0;
	sym->sym_count = 0;

	list_add_tail (&sym_root, (list_node_t *) sym);  // node as first member
	sym_count++;

	return sym;
	}


static void sym_sort ()
	{
	// Build index and sort

	list_node_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = (symbol_t *) node;  // node as first member

		index->key = sym->pos_count + sym->sym_count;  // number of duplicates
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
	}


// List the used symbols

static void sym_list ()
	{
	double entropy = 0.0;

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		uint_t sym_dup = sym->pos_count + sym->sym_count;

		double p = (double) sym_dup / (pos_count + sym_count);
		entropy += -p * log2 (p);

		printf ("[%u] base=%u ", i, sym->base);

		if (sym->size == 1)
			printf ("code=%hu", frame_in [sym->base]);
		else
			printf ("size=%u", sym->size);

		printf (" pos=%u sym=%u\n", sym->pos_count, sym->sym_count);
		}

	printf ("\nsymbol count=%u\n", sym_count);
	printf ("entropy=%f\n\n", entropy);
	}


// Scan frame for all base symbols

static void scan_base ()
	{
	list_init (&sym_root);
	list_init (&pos_root);

	memset (index_sym, 0, sizeof (index_sym_t) * CODE_MAX);

	// Count symbol occurrences

	for (uint_t i = 0; i < size_in; i++)
		{
		index_sym_t * index = index_sym + (uint_t) frame_in [i];
		symbol_t * sym = index->sym;

		if (!sym)
			{
			sym = sym_add ();

			sym->base = i;
			sym->size = 1;

			sym->left = NULL;
			sym->right = NULL;

			index->sym = sym;
			}

		position_t * pos = malloc (sizeof (position_t));
		list_add_tail (&pos_root, (list_node_t *) pos);  // node as first member

		pos->sym = sym;
		pos->pair = NULL;

		sym->pos_count++;
		}

	pos_count = size_in;
	}


// Scan frame for new pairs

static void scan_pair ()
	{
	list_node_t * node_left = pos_root.next;
	while ((node_left != pos_root.prev) && (node_left != &pos_root))
		{
		list_node_t * node_left_next = node_left->next;

		position_t * pos_left = (position_t *) node_left;  // node as first member
		if (!pos_left->pair)  // skip already found pair
			{
			// Add new pair

			pair_t * pair = malloc (sizeof (pair_t));
			list_add_tail (&pair_root, (list_node_t *) pair);  // node as first member

			pair->count = 1;

			pair->left = pos_left->sym;
			pair->right = ((position_t *) node_left_next)->sym;

			pos_left->pair = pair;  // pair now found there

			// Scan for pair duplicates

			list_node_t * node_right = node_left->next;
			while ((node_right != pos_root.prev) && (node_right != &pos_root))
				{
				list_node_t * node_right_next = node_right->next;

				position_t * pos_right = (position_t *) node_right;  // node as first member

				if (!pos_right->pair &&  // skip already found pair
					(pair->left == pos_right->sym) &&
					(pair->right == ((position_t *) node_right_next)->sym))
					{
					pos_right->pair = pair;  // pair now found there

					pair->count++;
					}

				node_right = node_right_next;
				}
			}

		node_left = node_left_next;
		}
	}


static void dec_pair (pair_t * pair)
	{
	if (pair)
		{
		pair->count--;

		if (!pair->count)
			{
			list_remove ((list_node_t *) pair);  // node as first member
			free (pair);
			}
		}
	}

// Crunch all occurrences of one pair

static int crunch_pair (pair_t * pair)
	{
	int shrink = 0;  // no shrink

	// Check all pair occurrences

	symbol_t * sym = NULL;

	list_node_t * node = pos_root.next;
	list_node_t * node_prev = &pos_root;

	while ((node != pos_root.prev) && (node != &pos_root))
		{
		list_node_t * node_next = node->next;

		position_t * pos = (position_t *) node;  // node as first member
		if (pos->pair == pair)
			{
			// Consider previous pair

			if (node_prev != &pos_root)
				{
				position_t * pos_prev = (position_t *) node_prev;  // node as first member
				dec_pair (pos_prev->pair);
				pos_prev->pair = NULL;
				}

			// Consider next pair

			if ((node_next != pos_root.prev) && (node_next != &pos_root))
				{
				position_t * pos_next = (position_t *) node_next;  // node as first member
				dec_pair (pos_next->pair);
				pos_next->pair = NULL;
				}

			// Replace current pair by new symbol

			symbol_t * sym_left = pair->left;
			symbol_t * sym_right = pair->right;

			if (!sym)
				{
				sym = sym_add ();

				sym->base = sym_left->base;
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

			// Shift frame end to left

			list_node_t * node_after = node_next->next;
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


static void crunch_word ()
	{
	// Iterate on pair scan & crunch

	list_init (&pair_root);

	while (1)
		{
		scan_pair ();

		// Look for the pair the most duplicated

		uint_t count_max = 0;
		pair_t * pair_max = NULL;

		list_node_t * node = pair_root.next;
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


// Compression with "base" (no compression)

static void compress_b ()
	{
	list_node_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;
		out_byte (frame_in [sym->base]);
		node = node->next;
		}
	}


// Decompression with "base" (no decompression)

static void expand_b ()
	{
	for (uint_t i = 0; i < size_in; i++)
		{
		out_byte (frame_in [i]);
		}
	}


// Compression with "prefixed base"

static void compress_pb ()
	{
	if (!opt_sym) sym_sort ();

	out_prefix (sym_count);

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		out_code (frame_in [sym->base], 8);
		}

	out_prefix (pos_count);

	list_node_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;
		out_prefix (sym->index);
		node = node->next;
		}

	out_pad ();
	}


// Decompression with "prefixed base"

static void expand_pb ()
	{
	uint_t count = in_prefix ();

	list_init (&sym_root);

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = sym_add ();
		sym->code = in_code (8);
		index->sym = sym;
		}

	count = in_prefix ();

	for (uint_t p = 0; p < count; p++)
		{
		uint_t i = in_prefix ();
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		out_byte (sym->code);
		}
	}


static void expand_sym (symbol_t * sym)
	{
	if (!sym->left)
		{
		frame_out [size_out++] = frame_in [sym->base];
		}
	else
		{
		expand_sym (sym->left);
		expand_sym (sym->right);
		}
	}


static void expand ()
	{
	size_out = 0;

	list_node_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;
		expand_sym (sym);

		node = node->next;
		}
	}


int main (int argc, char * argv [])
	{
	clock_t clock_begin = clock ();

	while (1)
		{
		char opt;

		while (1)
			{
			opt = getopt (argc, argv, "cesv");
			if (opt < 0 || opt == '?') break;

			switch (opt)
				{
				case 'c':  // compress
					opt_compress = 1;
					break;

				case 'e':  // expand
					opt_expand = 1;
					break;

				case 's':  // list symbols
					opt_sym = 1;
					break;

				case 'v':  // verbose
					opt_verb = 1;
					break;

				}
			}

		if (opt == '?' || optind != argc - 2 || (opt_compress == opt_expand))
			{
			printf ("usage: %s (-c | -e) [-sv] [input file] [output file]\n\n", argv [0]);
			puts ("  -c  compress");
			puts ("  -e  expand");
			puts ("  -s  list symbols");
			puts ("  -v  verbose");
			break;
			}

		in_frame (argv [optind]);

		if (opt_compress)
			{
			if (opt_verb)
				{
				puts ("INITIAL");
				printf ("frame length=%u\n\n", size_in);
				}

			if (size_in < 3)
				error (1, 0, "frame too short");

			scan_base ();

			if (opt_sym)
				{
				sym_sort ();
				sym_list ();
				}

			if (opt_verb) printf ("Compressing...");

			/*
			shrink_frame ();
			*/

			//compress_b ();
			compress_pb ();

			if (opt_verb) puts (" DONE\n");

			if (opt_sym)
				{
				//sym_sort ();
				sym_list ();
				}

			if (opt_verb)
				{
				puts ("FINAL");
				printf ("frame length=%u\n", size_out);

				double ratio = (double) size_out / size_in;
				printf ("ratio=%f\n\n", ratio);
				}

			out_frame (argv [optind + 1]);
			break;
			}

		if (opt_expand)
			{
			//expand_b ();
			expand_pb ();

			out_frame (argv [optind + 1]);
			break;
			}

		break;
		}

	clock_t clock_end = clock ();
	if (opt_verb) printf ("elapsed=%lu msecs\n\n", (clock_end - clock_begin) / 1000);

	return 0;
	}
