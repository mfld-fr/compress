//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <error.h>
#include <math.h>
#include <unistd.h>

#include "list.h"

typedef unsigned char uchar_t;
typedef unsigned int uint_t;


// Frame of codes

#define CODE_MAX 256  // 8 bits
#define FRAME_MAX 65536  // 64K

static uchar_t frame_in [FRAME_MAX];
static uint_t size_in;

static uchar_t frame_out [FRAME_MAX];
static uint_t size_out;

// Symbol definitions

struct symbol_s
	{
	uint_t count;  // number of symbol occurrences

	// For leaf symbol

	uint_t base;   // offset of first occurrence in input frame
	uint_t size;   // size in input codes

	// For node symbol

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct symbol_s symbol_t;

#define SYMBOL_MAX 65536  // 64K

// TODO: move symbols from static to dynamic

static symbol_t symbols [SYMBOL_MAX];
static uint_t sym_count;

struct pair_s;

struct position_s
	{
	list_node_t node;  // must be the first element

	symbol_t * sym;
	struct pair_s * pair;
	};

typedef struct position_s position_t;

static list_node_t pos_root;
static uint_t pos_size;

#define PAIR_MAX (SYMBOL_MAX - CODE_MAX)

struct pair_s
	{
	uint_t count;

	position_t * base;  // position of first occurrence

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct pair_s pair_t;

static pair_t pairs [PAIR_MAX];
static uint_t pair_count;


// Indexes for quick sort

struct index_sym_s
	{
	uint_t key;
	symbol_t * sym;
	};

typedef struct index_sym_s index_sym_t;

static index_sym_t index_sym [SYMBOL_MAX];

struct index_pair_s
	{
	uint_t key;
	pair_t * pair;
	};

typedef struct index_pair_s index_pair_t;

static index_pair_t index_pair [PAIR_MAX];


// Program options

uchar_t opt_sym_list;


static int sym_comp (const void * v1, const void * v2)
	{
	index_sym_t * i1 = (index_sym_t * ) v1;
	index_sym_t * i2 = (index_sym_t * ) v2;

	uint_t k1 = i1->key;
	uint_t k2 = i2->key;

	return (k1 < k2) ? 1 : ((k1 > k2) ? -1 : 0);
	}

static int pair_comp (const void * v1, const void * v2)
	{
	index_pair_t * i1 = (index_pair_t * ) v1;
	index_pair_t * i2 = (index_pair_t * ) v2;

	uint_t k1 = i1->key;
	uint_t k2 = i2->key;

	return (k1 < k2) ? 1 : ((k1 > k2) ? -1 : 0);
	}


static symbol_t * sym_add (uint_t base, uint_t size)
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * s = symbols + sym_count++;

	memset (s, 0, sizeof (symbol_t));  // erase if previously used

	s->size = size;
	s->base = base;

	return s;
	}


static void sym_list ()
	{
	// Build index and sort

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * s = symbols + i;

		index->key = s->count;
		index->sym = s;
		}

	qsort (index_sym, sym_count, sizeof (index_sym_t), sym_comp);

	// List the used symbols

	uint_t count = 0;
	double entropy = 0.0;

	for (uint_t i = 0; i < sym_count; i++)
		{
		index_sym_t * k = index_sym + i;
		symbol_t * s = k->sym;

		if (!s->count) break;

		count++;

		double p = (double) s->count / pos_size;
		entropy += -p * log2 (p);

		printf ("symbol [%u]: ", i);

		if (s->size == 1)
			printf ("code=%hu ", frame_in [s->base]);
			else
			printf ("size=%u ", s->size);

		printf ("count=%u\n", s->count);
		}

	printf ("\nused symbols=%u\n", count);
	printf ("entropy=%f\n\n", entropy);
	}


// Scan frame for all base symbols

static void scan_base ()
	{
	memset (symbols, 0, sizeof symbols);
	list_init (&pos_root);

	// Count symbol occurrences

	for (uint_t i = 0; i < size_in; i++)
		{
		symbol_t * sym = symbols + (uint_t) frame_in [i];

		if (!sym->count)
			{
			sym->base = i;
			sym->size = 1;
			}

		position_t * pos = malloc (sizeof (position_t));
		list_add_tail (&pos_root, &(pos->node));
		pos->sym = sym;

		sym->count++;
		}

	sym_count = CODE_MAX;
	pos_size = size_in;
	}


static pair_t * pair_add (position_t * base)
	{
	if (pair_count >= PAIR_MAX)
		error (1, 0, "too many pairs");

	pair_t * pair = pairs +  pair_count++;

	pair->base = base;
	pair->count = 1;

	return pair;
	}


// Scan frame for all pairs

static void scan_pair ()
	{
	list_node_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		pos->pair = NULL;
		node = node->next;
		}

	memset (pairs , 0, sizeof pairs);
	pair_count = 0;

	list_node_t * node_left = pos_root.next;
	while (node_left != pos_root.prev->prev)
		{
		position_t * pos_left = (position_t *) node_left;  // node as first member
		if (!pos_left->pair)  // skip already found pair
			{
			// Add new pair

			pair_t * pair = pair_add (pos_left);

			pair->left = pos_left->sym;
			pair->right = ((position_t *) (pos_left->node.next))->sym;

			pos_left->pair = pair;  // pair now found there

			// Scan for pair duplicates

			list_node_t * node_right = node_left->next;
			while (node_right != pos_root.prev)
				{
				position_t * pos_right = (position_t *) node_right;  // node as first member

				if (!pos_right->pair &&  // skip already found pair
					(pair->left == pos_right->sym) &&
					(pair->right == ((position_t *) (pos_right->node.next))->sym))
					{
					pos_right->pair = pair;  // pair now found there

					pair->count++;
					}

				node_right = node_right->next;
				}
			}

		node_left = node_left->next;
		}
	}


static int shrink_pair ()
	{
	int shrink = 0;  // no shrink

	// Build pair index and sort by occurrences
	// TODO: is this really necessary - not the maximum ?

	for (uint_t i = 0; i < pair_count; i++)
		{
		index_pair_t * index = index_pair + i;
		pair_t * pair = pairs + i;

		index->key = pair->count;
		index->pair = pair;
		}

	qsort (index_pair, pair_count, sizeof (index_pair_t), pair_comp);

	// Try to shrink all repeated pairs

	for (uint_t i = 0; i < pair_count; i++)
		{
		index_pair_t * k = index_pair + i;
		pair_t * pair = k->pair;

		// Skip the symmetric pairs

		if (pair->left == pair->right) continue;

		// Skip any singleton or invalid pairs

		if (pair->count < 2) continue;

		// Check all pair occurrences

		symbol_t * sym = NULL;

		list_node_t * node = (list_node_t *) pair->base;  // node as first member
		while ((node != pos_root.prev) && (node != &pos_root))  // also check shifted limit
			{
			position_t * pos = (position_t *) node;  // node as first member
			if (pos->pair == pair)
				{
				// Consider left neighbor pair

				pair_t * pair_left = NULL;
				uint_t count_left = 0;

				if (node != pos_root.next)
					{
					pair_left = ((position_t *) (node->prev))->pair;

					// Filter symmetric pairs

					if (pair_left && (pair_left->left != pair_left->right))
						count_left = pair_left->count;

					}

				// Consider right neighbor pair

				pair_t * pair_right = NULL;
				uint_t count_right = 0;
				if (node != pos_root.prev->prev)
					{
					pair_right = ((position_t *) (node->next))->pair;

					// Filter symmetric pairs

					if (pair_right && (pair_right->left != pair_right->right))
						count_right = pair_right->count;

					}

				// Can shrink if stronger than neighbors

				if ((count_left <= pair->count) && (pair->count >= count_right))
					{
					// Invalidate neighbor pairs

					if (pair_left) pair_left->count = 0;
					if (pair_right) pair_right->count = 0;

					// Create a new symbol from the pair

					if (!sym)
						{
						sym = sym_add (0, 2);

						sym->left = pair->left;
						sym->right = pair->right;
						}

					// Update symbol counts

					sym->left->count--;
					sym->right->count--;

					sym->count++;

					// Replace current pair by the new symbol
					// and shift the frame left

					pos->sym = sym;
					pos->pair = NULL;  // no more pair here

					list_remove (node->next);
					pos_size--;

					shrink = 1;
					}
				}

			node = node->next;
			}
		}

	return shrink;
	}


static void compress ()
	{
	// Base symbol scan

	scan_base ();

	// Iterate on pair scan & shrink

	while (1)
		{
		scan_pair ();
		if (!shrink_pair ()) break;
		}
	}


static void exp_sym (symbol_t * s)
	{
	if (s->size == 1)
		{
		frame_out [size_out++] = frame_in [s->base];
		}
	else
		{
		exp_sym (s->left);
		exp_sym (s->right);
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
		exp_sym (sym);

		node = node->next;
		}
	}


int main (int argc, char * argv [])
	{
	FILE * f = NULL;

	while (1)
		{
		char opt;

		while (1)
			{
			opt = getopt (argc, argv, "s");
			if (opt < 0 || opt == '?') break;

			switch (opt)
				{
				case 's':  // dump symbols
					opt_sym_list = 1;
					break;

				}
			}

		if (opt == '?' || optind != argc - 1)
			{
			printf ("usage: %s [options] [input file]\n\n", argv [0]);
			puts ("  -s  list input symbols");
			break;
			}

		f = fopen (argv [optind], "r");
		if (!f) break;

		while (1)
			{
			int c = fgetc (f);
			if (c < 0 || c > 255) break;

			if (size_in >= FRAME_MAX)
				error (1, 0, "frame too long");

			frame_in [size_in++] = c;
			}

		puts ("INITIAL:");
		printf ("frame length=%u\n\n", size_in);

		if (opt_sym_list)
			{
			scan_base ();
			sym_list ();
			}
		else
			{
			if (size_in < 3)
				error (1, 0, "frame too short");

			printf ("Compressing...");
			clock_t clock_begin = clock ();
			compress ();
			clock_t clock_end = clock ();
			puts (" DONE\n");

			puts ("COMPRESS:");
			printf ("frame length=%u\n", pos_size);

			double ratio = (double) pos_size / size_in;
			printf ("ratio=%f\n", ratio);
			printf ("elapsed=%lu usecs\n\n", (clock_end - clock_begin));

			puts ("EXPAND:");

			clock_begin = clock ();
			expand ();
			clock_end = clock ();

			if (size_out != size_in)
				error (1, 0, "length mismatch");

			if (memcmp (frame_in, frame_out, size_out))
				error (1, 0, "frame mismatch");

			printf ("elapsed=%lu usecs\n", (clock_end - clock_begin));
			}

		break;
		}

	if (f) fclose (f);

	return 0;
	}
