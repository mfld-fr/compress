//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <error.h>
#include <math.h>
#include <unistd.h>

#include "list.h"
#include "stream.h"


// Symbol definitions

#define SYMBOL_MAX 65536  // 64K

struct symbol_s
	{
	list_t node;  // must be the first member

	uint_t pos_count;  // number of occurrences in the frame
	uint_t sym_count;  // number of occurrences in the tree

	uchar_t code;  // base byte code (primary symbol)
	uint_t index;  // index after sort
	uint_t rep;    // repeat counter

	uint_t base;  // offset of first occurrence in input frame
	uint_t size;  // size in input codes

	// For node symbol (secondary)

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct symbol_s symbol_t;

static list_t sym_root;
static uint_t sym_count;


// Pair definitions

struct pair_s;

struct position_s
	{
	list_t node;  // must be the first member

	uint_t base;

	symbol_t * sym;
	struct pair_s * pair;
	};

typedef struct position_s position_t;

static list_t pos_root;
static uint_t pos_count;


struct pair_s
	{
	list_t node;  // must be the first member

	uint_t count;  // number of occurrences in the frame

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct pair_s pair_t;

static list_t pair_root;


// Indexes for quick sort

#define SORT_ALL 0  // all symbols
#define SORT_REP 1  // filter repeat out
#define SORT_DUP 2  // duplicated only

struct index_sym_s
	{
	uint_t key;
	symbol_t * sym;
	};

typedef struct index_sym_s index_sym_t;

static index_sym_t index_sym [SYMBOL_MAX];


// Element definition
// Used for decompression

struct elem_s
	{
	uint_t base;
	uint_t size;
	};

typedef struct elem_s elem_t;

static elem_t elements [SYMBOL_MAX];

static uint_t patterns [FRAME_MAX];
static uint_t patt_len;


// Program options

#define ALGO_DEF 0
#define ALGO_BASE 1
#define ALGO_REP_BASE 2
#define ALGO_PREF 3
#define ALGO_REP_PREF 4
#define ALGO_SYM 5
#define ALGO_REP_SYM 6

uchar_t opt_algo;
uchar_t opt_compress;
uchar_t opt_expand;
uchar_t opt_sym;
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
	memset (sym, 0, sizeof (symbol_t));

	list_add_tail (&sym_root, (list_t *) sym);  // node as first member
	sym_count++;

	return sym;
	}


// Build index and sort
// TODO: build key in callback

static uint_t sym_sort (uint_t filter)
	{
	uint_t filt_count = 0;

	list_t * node = sym_root.next;
	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = (symbol_t *) node;  // node as first member

		uint_t count = sym->pos_count + sym->sym_count;  // number of duplicates

		switch (filter)
			{
			case SORT_ALL:
				index->key = count + ((sym->rep > 1) ? sym->rep : 0);
				filt_count++;
				break;

			case SORT_REP:
				if (sym->rep > 1)
					{
					index->key = 0;
					break;
					}

				index->key = count;
				filt_count++;
				break;

			case SORT_DUP:
				if (sym->size == 1 || (count == 1 && sym->rep != 1))
					{
					index->key = 0;
					break;
					}

				index->key = count;
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

static void sym_list ()
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

		if (sym->rep > 1)
			printf (" rep=%u", sym->rep);
		else
			printf (" pos=%u", sym->pos_count);

		printf (" sym=%u\n", sym->sym_count);
		}

	printf ("\nentropy=%f\n\n", entropy);
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

		sym->pos_count++;
		}

	pos_count = size_in;
	}


// Scan frame for new pairs

// TODO: optimize the pair scan with a sublist
// initialized by scan_base and updated by crunch_pair

static void scan_pair ()
	{
	list_t * node_left = pos_root.next;
	while ((node_left != pos_root.prev) && (node_left != &pos_root))
		{
		list_t * node_left_next = node_left->next;

		position_t * pos_left = (position_t *) node_left;  // node as first member
		if (!pos_left->pair)  // skip already found pair
			{
			// Add new pair

			pair_t * pair = malloc (sizeof (pair_t));
			list_add_tail (&pair_root, (list_t *) pair);  // node as first member

			pair->count = 1;

			pair->left = pos_left->sym;
			pair->right = ((position_t *) node_left_next)->sym;

			pos_left->pair = pair;  // pair now found there

			// Scan for pair duplicates

			list_t * node_right = node_left->next;
			while ((node_right != pos_root.prev) && (node_right != &pos_root))
				{
				list_t * node_right_next = node_right->next;

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
			list_remove ((list_t *) pair);  // node as first member
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

static void crunch_rep ()
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

				sym_rep->rep = 1;
				sym_left->rep = 1;  // repeated

				sym_rep->base = pos_left->base;
				sym_rep->size = sym_left->size;

				sym_rep->left = sym_left;
				sym_left->sym_count++;
				}

			sym_left->pos_count--;

			sym_rep->rep++;

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
// Algorithms
//------------------------------------------------------------------------------

// Compression with "base" (no compression)
// Just for testing

static void compress_b ()
	{
	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;
		out_byte (sym->code);

		node = node->next;
		}
	}


// Decompression with "base" (no decompression)
// Just for testing

static void expand_b ()
	{
	for (uint_t i = 0; i < size_in; i++)
		{
		out_byte (in_byte ());
		}
	}


// Compression with "repeated base"
// Just for testing

static void compress_rb ()
	{
	crunch_rep ();

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		if (sym->rep > 1)
			{
			out_bit (1);  // repeat flag

			out_pref_odd (sym->rep - 2);

			sym = sym->left;
			}
		else
			{
			out_bit (0);
			}

		out_code (sym->code, 8);

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "repeated base"
// Just for testing

static void expand_rb ()
	{
	uint_t count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < count; p++)
		{
		if (!in_bit ())  // code flag
			{
			out_byte (in_code (8));
			}
		else
			{
			uint_t rep = 2 + in_pref_odd ();
			uchar_t code = in_code (8);
			while (rep--) out_byte (code);
			}
		}
	}


// Compression with "prefixed base"
// Just for testing

static void compress_pb ()
	{
	if (!opt_sym) sym_sort (SORT_ALL);

	// No more than 6 prefixed bits to save space
	// so no more than 14 indexed symbols

	uint_t count = (sym_count < 14) ? sym_count : 14;

	out_pref_odd (count - 1);

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		out_code (sym->code, 8);
		}

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		// Use index only when space gain

		if (sym->index < count)
			{
			out_bit (1);  // index flag
			out_pref_even (sym->index);
			}
		else
			{
			out_bit (0);  // code flag
			out_code (sym->code, 8);
			}

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "prefixed base"
// Just for testing

static void expand_pb ()
	{
	list_init (&sym_root);

	uint_t count = 1 + in_pref_odd ();

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = sym_add ();
		sym->code = in_code (8);
		index->sym = sym;
		}

	count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < count; p++)
		{
		if (in_bit ())  // index flag
			{
			uint_t i = in_pref_even ();
			index_sym_t * index = index_sym + i;
			symbol_t * sym = index->sym;
			out_byte (sym->code);
			}
		else
			{
			out_byte (in_code (8));
			}
		}
	}


// Compression with "repeated prefixed base"
// Just for testing

static void compress_rpb ()
	{
	crunch_rep ();

	uint_t count = sym_sort (SORT_REP);

	// No more than 6 prefixed bits to save space
	// so no more than 14 indexed symbols

	count = (count < 14) ? count : 14;

	out_pref_odd (count - 1);

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;
		out_code (sym->code, 8);
		}

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		uchar_t rep = 0;

		if (sym->rep > 1)
			{
			out_bit (1);  // repeat word
			out_bit (0);

			out_pref_odd (sym->rep - 2);

			sym = sym->left;

			rep = 1;
			}

		if (sym->index < 14)
			{
			out_bit (1); // index flag or word
			if (!rep) out_bit (1);

			out_pref_even (sym->index);
			}
		else
			{
			out_bit (0);  // code flag and word
			out_code (sym->code, 8);
			}

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "repeated prefixed base"
// Just for testing

static void expand_rpb ()
	{
	list_init (&sym_root);

	uint_t count = 1 + in_pref_odd ();

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = sym_add ();
		sym->code = in_code (8);
		index->sym = sym;
		}

	count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < count; p++)
		{
		if (!in_bit ())  // code word
			{
			out_byte (in_code (8));
			}
		else
			{
			uint_t rep = 1;

			if (!in_bit ())  // repeat word
				{
				rep = 2 + in_pref_odd ();

				if (in_bit ())  // index flag
					{
					uint_t i = in_pref_even ();
					index_sym_t * index = index_sym + i;
					symbol_t * sym = index->sym;
					while (rep--) out_byte (sym->code);
					}
				else
					{
					uchar_t code = in_code (8);
					while (rep--) out_byte (code);
					}
				}
			else
				{
				uint_t i = in_pref_even ();
				index_sym_t * index = index_sym + i;
				symbol_t * sym = index->sym;
				out_byte (sym->code);
				}
			}
		}
	}


// Compression with "symbol"

static void compress_s ()
	{
	error (1, 0, "not implemented");
	}


// Decompression with "symbol"

static void expand_s ()
	{
	error (1, 0, "not implemented");
	}


// Compression with "repeated symbol"

static uint_t walk_sym_len (symbol_t * sym, uint_t len);
static void walk_sym (symbol_t * sym, uchar_t len);


static uint_t walk_child_len (symbol_t * sym, uint_t len)
	{
	if (sym->size == 1 || (sym->sym_count == 1 && sym->pos_count == 0 && sym->rep != 1))
		{
		len = walk_sym_len (sym, len);
		}
	else
		{
		len++;
		}

	return len;
	}


static void walk_child (symbol_t * sym, uchar_t len)
	{
	if (sym->size == 1 || (sym->sym_count == 1 && sym->pos_count == 0 && sym->rep != 1))
		{
		walk_sym (sym, len);
		}
	else
		{
		out_bit (1);  // index
		out_code (sym->index, len);
		}
	}


static uint_t walk_sym_len (symbol_t * sym, uint_t len)
	{
	if (sym->size == 1)
		{
		len++;
		}
	else
		{
		len = walk_child_len (sym->left, len);
		len = walk_child_len (sym->right, len);
		}

	return len;
	}


static void walk_sym (symbol_t * sym, uchar_t len)
	{
	if (sym->size == 1)
		{
		out_bit (0);  // code
		out_code (sym->code, 8);
		}
	else
		{
		walk_child (sym->left, len);
		walk_child (sym->right, len);
		}
	}


static void compress_rs ()
	{
	crunch_word ();
	crunch_rep ();

	uint_t count = sym_sort (SORT_DUP);
	uchar_t len = log2u (count);

	out_pref_odd (count - 1);

	for (uint_t i = 0; i < count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		out_pref_odd (walk_sym_len (sym, 0) - 2);
		walk_sym (sym, len);
		}

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		uint_t rep = 1;
		if (sym->rep > 1)
			{
			rep = sym->rep;
			sym = sym->left;

            // TODO: use code, repeat and insert symbols

			out_bit (1);   // repeat
			out_pref_odd (rep - 2);
			}
		else
			{
			out_bit (0);  // no repeat
			}

		walk_child (sym, len);

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "repeated symbol"

static void walk_elem (uint_t i)
	{
	elem_t * elem  = elements + i;
	uint_t base = elem->base;

	for (uint i = 0; i < elem->size; i++)
		{
		uint_t patt = patterns [base++];
		if (patt & 32768)
			walk_elem (patt & 32767);
		else
			out_byte (patt);

		}
	}


static void expand_rs ()
	{
	uint_t count = 1 + in_pref_odd ();
	uchar_t len = log2u (count);

	for (uint_t i = 0; i < count; i++)
		{
		elem_t * elem = elements + i;

		uint_t size = 2 + in_pref_odd ();

		elem->size = size;
		elem->base = patt_len;

		for (uint_t j = 0; j < size; j++)
			{
			if (in_bit ())  // index
				patterns [patt_len++] = 32768 | in_code (len);
			else
				patterns [patt_len++] = in_code (8);

			}
		}

	count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < count; p++)
		{
		uint_t rep = 1;
		if (in_bit ()) // repeat
			rep = 2 + in_pref_odd ();

		if (in_bit ())  // index
			{
			uint_t i = in_code (len);
			while (rep--) walk_elem (i);
			}
		else
			{
			uchar_t code = in_code (8);
			while (rep--) out_byte (code);
			}
		}
	}


//------------------------------------------------------------------------------
// Main entry point
//------------------------------------------------------------------------------

int main (int argc, char * argv [])
	{
	clock_t clock_begin = clock ();

	while (1)
		{
		char opt;

		while (1)
			{
			opt = getopt (argc, argv, "cem:sv");
			if (opt < 0 || opt == '?') break;

			switch (opt)
				{
				case 'c':  // compress
					opt_compress = 1;
					break;

				case 'e':  // expand
					opt_expand = 1;
					break;

				case 'm':  // algorithm
					if (!strcmp (optarg, "b"))
						opt_algo = ALGO_BASE;
					else if (!strcmp (optarg, "rb"))
						opt_algo = ALGO_REP_BASE;
					else if (!strcmp (optarg, "pb"))
						opt_algo = ALGO_PREF;
					else if (!strcmp (optarg, "rpb"))
						opt_algo = ALGO_REP_PREF;
					else if (!strcmp (optarg, "s"))
						opt_algo = ALGO_SYM;
					else if (!strcmp (optarg, "rs"))
						opt_algo = ALGO_REP_SYM;
					else
						error (1, 0, "unknown algorithm");

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
			printf ("usage: %s (-c | -d) [-sv] [-m <algo>] <input file> <output file>\n\n", argv [0]);
			puts ("  -c  compress");
			puts ("  -e  expand");
			puts ("  -m  algorithm");
			puts ("  -s  list symbols");
			puts ("  -v  verbose");
			puts ("");
			puts ("algorithms:");
			puts ("  b    base (no compression)");
			puts ("  rb   repeat base");
			puts ("  pb   prefixed base");
			puts ("  rpb  repeat prefixed base");
			puts ("  s    symbol");
			puts ("  rs   repeat symbol (default)");
			puts ("");
			break;
			}

		in_frame (argv [optind]);

		if (opt_compress)
			{
			if (size_in < 3)
				error (1, 0, "frame too short");

			scan_base ();

			if (opt_verb)
				{
				puts ("INITIAL");
				printf ("frame length=%u\n", size_in);
				printf ("symbol count=%u\n\n", sym_count);
				}

			if (opt_sym)
				{
				sym_sort (SORT_ALL);
				sym_list ();
				}

			if (opt_verb) printf ("Compressing...");

			switch (opt_algo)
				{
				case ALGO_BASE:
					compress_b ();
					break;

				case ALGO_REP_BASE:
					compress_rb ();
					break;

				case ALGO_PREF:
					compress_pb ();
					break;

				case ALGO_REP_PREF:
					compress_rpb ();
					break;

				case ALGO_SYM:
					compress_s ();
					break;

				case ALGO_REP_SYM:
					compress_rs ();
					break;

				default:
					compress_rs ();
					break;

				}

			if (opt_verb) puts (" DONE\n");

			if (opt_sym)
				{
				sym_sort (SORT_ALL);
				sym_list ();
				}

			if (opt_verb)
				{
				puts ("FINAL");
				printf ("frame size=%u\n", pos_count);
				printf ("symbol count=%u\n", sym_count);
				printf ("frame length=%u\n", size_out);

				double ratio = (double) size_out / size_in;
				printf ("ratio=%f\n\n", ratio);
				}

			out_frame (argv [optind + 1]);
			break;
			}

		if (opt_expand)
			{
			if (opt_verb) printf ("Expanding...");

			switch (opt_algo)
				{
				case ALGO_BASE:
					expand_b ();
					break;

				case ALGO_REP_BASE:
					expand_rb ();
					break;

				case ALGO_PREF:
					expand_pb ();
					break;

				case ALGO_REP_PREF:
					expand_rpb ();
					break;

				case ALGO_SYM:
					expand_s ();
					break;

				case ALGO_REP_SYM:
					expand_rs ();
					break;

				default:
					expand_rs ();
					break;

				}

			if (opt_verb) puts (" DONE\n");

			out_frame (argv [optind + 1]);
			break;
			}

		break;
		}

	clock_t clock_end = clock ();
	if (opt_verb) printf ("elapsed=%lu usecs\n\n", clock_end - clock_begin);

	return 0;
	}


//------------------------------------------------------------------------------
