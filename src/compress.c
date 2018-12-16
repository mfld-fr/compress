//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <string.h>
#include <time.h>
#include <error.h>
#include <unistd.h>

#include "list.h"
#include "stream.h"
#include "symbol.h"


// Element definition
// Used for decompression

struct elem_s
	{
	uint_t base;
	uint_t size;
	};

typedef struct elem_s elem_t;

static elem_t elements [SYMBOL_MAX];
static uint_t elem_count;

static uint_t patterns [FRAME_MAX];
static uint_t patt_len;


// Program options

#define ALGO_DEF      0
#define ALGO_BASE     1
#define ALGO_REP_BASE 2
#define ALGO_PREF     3
#define ALGO_REP_PREF 4
#define ALGO_SYM_EXT  5
#define ALGO_SYM_INT  6
#define ALGO_REP_SYM  7

uchar_t opt_algo;
uchar_t opt_compress;
uchar_t opt_expand;
uchar_t opt_sym;
uchar_t opt_verb;


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

		if (sym->rep_count > 1)
			{
			out_bit (1);  // repeat flag

			out_pref_odd (sym->rep_count - 2);

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

		if (sym->rep_count > 1)
			{
			out_bit (1);  // repeat word
			out_bit (0);

			out_pref_odd (sym->rep_count - 2);

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


// Walking the symbol tree

static uint_t walk_sym_len (symbol_t * sym);
static void walk_sym (symbol_t * sym, uchar_t bit_len);
static void walk_sym_i (symbol_t * sym, uchar_t bit_len);


static uint_t walk_child_len (symbol_t * sym)
	{
	uint_t len;

	if (sym->size == 1 || (sym->sym_count == 1 && sym->pos_count == 0 && sym->rep_count != 1))
		{
		len = walk_sym_len (sym);
		}
	else
		{
		len = 1;  // reference
		}

	return len;
	}


static void walk_child (symbol_t * sym, uchar_t bit_len)
	{
	if (sym->size == 1 || (sym->sym_count == 1 && sym->pos_count == 0 && sym->rep_count != 1))
		{
		walk_sym (sym, bit_len);
		}
	else
		{
		out_bit (1);  // index
		out_code (sym->index, bit_len);
		}
	}


static void walk_child_i (symbol_t * sym, uchar_t bit_len)
	{
	if (sym->size == 1 || (sym->sym_count == 1 && sym->pos_count == 0 && sym->rep_count != 1))
		{
		walk_sym_i (sym, bit_len);
		}
	else
		{
		if (!sym->len)
			{
			out_bit (1);  // definition
			out_bit (0);

			out_pref_odd (walk_sym_len (sym) - 2);
			walk_sym_i (sym, bit_len);

			sym->index = index_count++;
			}
		else
			{
			out_bit (1);  // reference
			out_bit (1);

			out_code (sym->index, bit_len);
			}
		}
	}


static uint_t walk_sym_len (symbol_t * sym)
	{
	if (!sym->len)
		{
		if (sym->size == 1)
			{
			sym->len = 1;
			}
		else
			{
			sym->len = walk_child_len (sym->left);
			sym->len += walk_child_len (sym->right);
			}
		}

	return sym->len;
	}


static void walk_sym (symbol_t * sym, uchar_t bit_len)
	{
	if (sym->size == 1)
		{
		out_bit (0);  // code
		out_code (sym->code, 8);
		}
	else
		{
		walk_child (sym->left,  bit_len);
		walk_child (sym->right, bit_len);
		}
	}


static void walk_sym_i (symbol_t * sym, uchar_t bit_len)
	{
	if (sym->size == 1)
		{
		out_bit (0);  // code
		out_code (sym->code, 8);
		}
	else
		{
		walk_child_i (sym->left,  bit_len);
		walk_child_i (sym->right, bit_len);
		}
	}


// Walk the element tree

static void walk_elem (uint_t i)
	{
	elem_t * elem  = elements + i;
	uint_t base = elem->base;

	for (uint_t i = 0; i < elem->size; i++)
		{
		uint_t patt = patterns [base++];
		if (patt & 32768)
			walk_elem (patt & 32767);
		else
			out_byte (patt);

		}
	}


// Compression with "symbol"
// Prepended dictionary (external)

static void compress_se ()
	{
	crunch_word ();

	uint_t def_count = sym_sort (SORT_DUP);
	uchar_t bit_len = log2u (def_count);

	out_pref_odd (def_count - 1);

	for (uint_t i = 0; i < def_count; i++)
		{
		index_sym_t * index = index_sym + i;
		symbol_t * sym = index->sym;

		out_pref_odd (walk_sym_len (sym) - 2);
		walk_sym (sym, bit_len);
		}

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		walk_child (sym, bit_len);

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "symbol"
// Prepended dictionary (external)

static void expand_se ()
	{
	uint_t def_count = 1 + in_pref_odd ();
	uchar_t bit_len = log2u (def_count);

	for (uint_t i = 0; i < def_count; i++)
		{
		elem_t * elem = elements + i;

		uint_t size = 2 + in_pref_odd ();

		elem->size = size;
		elem->base = patt_len;

		for (uint_t j = 0; j < size; j++)
			{
			if (in_bit ())  // index
				patterns [patt_len++] = 32768 | in_code (bit_len);
			else
				patterns [patt_len++] = in_code (8);

			}
		}

	uint_t pos_count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < pos_count; p++)
		{
		if (in_bit ())  // index
			{
			uint_t i = in_code (bit_len);
			walk_elem (i);
			}
		else
			{
			uchar_t code = in_code (8);
			out_byte (code);
			}
		}
	}


// Compression with "symbol"
// Embedded dictionary (internal)

static void compress_si ()
	{
	crunch_word ();

	uint_t def_count = sym_sort (SORT_DUP);
	uchar_t bit_len = log2u (def_count);

	out_pref_odd (bit_len - 1);
	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		walk_child_i (sym, bit_len);

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "symbol"
// Embedded dictionary (internal)

static uint_t in_elem (uchar_t bit_len)
	{
	uint_t size;

	if (!in_bit ())  // byte code
		{
		uchar_t code = in_code (8);
		out_byte (code);
		size = 1;
		}
	else
		{
		if (in_bit ())  // reference
			{
			uint_t i = in_code (bit_len);
			elem_t * elem = elements + i;
			size = elem->size;

			memcpy (frame_out + size_out, frame_out + elem->base, size);

			size_out += size;
			}
		else
			{
			// definition

			uint_t base = size_out;
			size = 0;

			uint_t len = 2 + in_pref_odd ();

			for (uint_t i = 0; i < len; i++)
				size += in_elem (bit_len);

			// Parent element created after child

			elem_t * elem = elements + elem_count++;
			elem->base = base;
			elem->size = size;
			}
		}

	return size;
	}


static void expand_si ()
	{
	uchar_t bit_len = 1 + in_pref_odd ();

	uint_t pos_count = 1 + in_pref_odd ();

	for (uint_t p = 0; p < pos_count; p++)
		in_elem (bit_len);

	}


// Compression with "repeated symbol"

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

		out_pref_odd (walk_sym_len (sym) - 2);
		walk_sym (sym, len);
		}

	out_pref_odd (pos_count - 1);

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = (position_t *) node;  // node as first member
		symbol_t * sym = pos->sym;

		uint_t rep = 1;
		if (sym->rep_count > 1)
			{
			rep = sym->rep_count;
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
					else if (!strcmp (optarg, "se"))
						opt_algo = ALGO_SYM_EXT;
					else if (!strcmp (optarg, "si"))
						opt_algo = ALGO_SYM_INT;
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
			puts ("  se   symbol - prepend dictionary (external)");
			puts ("  si   symbol - embed dictionary (internal)");
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

				case ALGO_SYM_EXT:
					compress_se ();
					break;

				case ALGO_SYM_INT:
					compress_si ();
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

				case ALGO_SYM_EXT:
					expand_se ();
					break;

				case ALGO_SYM_INT:
					expand_si ();
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
