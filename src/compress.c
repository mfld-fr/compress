//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <string.h>
#include <time.h>
#include <error.h>
#include <unistd.h>
#include <limits.h>

#include "common.h"
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
#define ALGO_REP_SE   7

uchar opt_algo;
uchar opt_compress;
uchar opt_expand;
uchar opt_sym;
uchar opt_time;
uchar opt_verb;

static uint base_count;
static uint ref_count;
static uint rep_count;

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
		position_t * pos = structof (position_t, node, node);
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

	list_t * node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = structof (position_t, node, node);
		symbol_t * sym = pos->sym;

		if (sym->repeat)
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
	while (!in_eof ())
		{
		if (!in_bit (1))  // code flag
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
	sym_sort (SORT_USE);

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
		position_t * pos = structof (position_t, node, node);
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
		if (in_bit (1))  // index flag
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

	sym_sort (SORT_REP);

	// No more than 6 prefixed bits to save space
	// so no more than 14 indexed symbols

	uint count = (sym_count < 14) ? sym_count : 14;

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
		position_t * pos = structof (position_t, node, node);
		symbol_t * sym = pos->sym;

		uchar_t rep = 0;

		if (sym->repeat)
			{
			out_bit (1);  // repeat prefix
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
		if (!in_bit (1))  // code word
			{
			out_byte (in_code (8));
			}
		else
			{
			uint_t rep = 1;

			if (!in_bit (1))  // repeat word
				{
				rep = 2 + in_pref_odd ();

				if (in_bit (1))  // index flag
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
// Prepended dictionary (external)

static uchar out_child_se (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count, uchar pos);

static uchar out_sym_se (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count, uchar pos)
	{
	if (sym->keep)
		{
		// Insert the next flag inside a definition
		if (def_len) out_bit ((def_count == def_len) ? 0 : 1);

		if (pos) out_bit (1);
		out_bit (1);  // index
		out_code (sym->index, ref_bit);
		ref_count++;

		if (def_len) def_count++;
		}
	else
		{
		def_count = out_child_se (sym, ref_bit, def_len, def_count, pos);
		}

	return def_count;
	}

static uchar out_child_se (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count, uchar pos)
	{
	if (sym->size == 1)
		{
		// Insert next flag inside a definition
		// No flag in a definition with a single base symbol
		if (def_len > 1) out_bit ((def_count == def_len) ? 0 : 1);

		out_bit (0);  // base
		out_code (sym->code, 8);
		base_count++;

		if (def_len) def_count++;
		}
	else
		{
		def_count = out_sym_se (sym->left, ref_bit, def_len, def_count, pos);
		def_count = out_sym_se (sym->right, ref_bit, def_len, def_count, pos);
		}

	return def_count;
	}


// Walk the element tree

#define PATTERN_MAX (32768)

static uint last_elem = 0;
static uint depth = 0;

static void walk_elem (uint_t i)
	{
	depth++;
	if (last_elem == i && depth > 1)
		{
		puts ("HELP !");
		}

	elem_t * elem  = elements + i;
	uint_t base = elem->base;

	for (uint_t j = 0; j < elem->size; j++)
		{
		uint_t patt = patterns [base++];
		if (patt & PATTERN_MAX)
			{
			last_elem = i;
			walk_elem (patt & (PATTERN_MAX - 1));
			}
		else
			out_byte (patt);

		}
	depth--;
	}


static void compress_se ()
	{
	crunch_word ();

	if (opt_sym)
		{
		sym_sort (SORT_ALL);
		sym_list (LIST_ALL);
		}

	// First consider that all the duplicated symbols are worth to keep
	// and compute the number of bits to reference all that symbols

	keep_count = keep_dup ();

	if (opt_verb) printf ("Duplicated symbols: %u\n\n", keep_count);
	uchar ref_bit = log2u (keep_count - 1);

	uchar best_bit = UCHAR_MAX;
	uint best_keep = UINT_MAX;
	uint min_cost = UINT_MAX;

	// Iterate on the reference bits down to 0 to get the best one

	list_t * node;

	while (ref_bit > 0)
		{
		if (opt_verb) printf ("Reference bits: %u\n", ref_bit);

		// Compute the symbol costs

		keep_count = 0;

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			sym_cost_se (sym, ref_bit, 1);  // select
			node = node->next;
			}

		if (opt_verb) printf ("Worth symbols: %u\n", keep_count);

		// Discard worthless

		sym_sort (SORT_GAIN);

		uint keep_max = 1 << ref_bit;
		if (keep_count > keep_max)
			{
			for (uint i = keep_max; i < keep_count; i++)
				{
				index_sym_t * index = index_sym + i;
				symbol_t * sym = index->sym;
				sym->keep = 0;
				}

			keep_count = keep_max;
			}

		if (opt_verb) printf ("Kept symbols: %u\n", keep_count);

		// Recompute the symbol costs

		uint cost = 0;
		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			cost += sym_cost_se (sym, ref_bit, 0);  // no select
			node = node->next;
			}

		if (opt_verb) printf ("Total cost: %u bytes\n\n", cost / 8);

		// No need to go further when cost increases
		if (cost >= min_cost) break;

		best_keep = keep_count;
		min_cost = cost;
		best_bit = ref_bit;

		// Save the best selection

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			sym->best_keep = sym->keep;
			sym->best_len = sym->len;
			node = node->next;
			}

		ref_bit--;
		}

	if (opt_verb) printf ("Best bits: %u\n\n", best_bit);

	// Restore the best selection

	node = sym_root.next;
	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);
		sym->keep = sym->best_keep;
		sym->len = sym->best_len;
		node = node->next;
		}

	// FIXME: truncating above to fit the reference bits
	// discard some symbols with better gain than the kept ones.
	// This can be seen by sorting again the symbol by gain,
	// as some indexes are skipped in the list.
	// sym_sort (SORT_GAIN);
	// sym_list (LIST_KEEP);

	// Output symbol dictionary

	out_pref_odd (best_keep - 1);

	node = sym_root.next;
	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);
		if (sym->keep)
			{
			out_child_se (sym, best_bit, sym->len, 1, 0);  // inside a definition
			sym->index = index_count++;
			}

		node = node->next;
		}

	// Output frame

	node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = structof (position_t, node, node);
		out_sym_se (pos->sym, best_bit, 0, 0, 0);  // outside a definition
		node = node->next;
		}

	out_pad ();
	}


// Decompression with "symbol"
// Prepended dictionary (external)

static void expand_se ()
	{
	uint_t def_count = 1 + in_pref_odd ();
	uchar_t ref_bit = log2u (def_count - 1);

	for (uint_t i = 0; i < def_count; i++)
		{
		elem_t * elem = elements + i;

		elem->base = patt_len;

		// Iterate until next flag is false

		uchar count = 0;
		while (1)
			{
			uchar flag = in_bit (0);  // no shift - keep bit in input
			// No next flag in a definition with a single base symbol
			if (count || flag) in_bit (1);

			if (in_bit (1))  // index
				patterns [patt_len++] = PATTERN_MAX | in_code (ref_bit);
			else
				patterns [patt_len++] = in_code (8);

			if (!flag) break;  // was last symbol
			count++;
			}

		elem->size = count + 1;
		}

	while (1)
		{
		if (in_eof ()) break;

		if (in_bit (1))  // index
			{
			uint_t i = in_code (ref_bit);
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

static uchar out_child_si (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count);

static uchar out_sym_si (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count)
	{
	if (sym->keep)
		{
		// Insert the next flag inside a definition
		if (def_len) out_bit ((def_count == def_len) ? 0 : 1);

		if (sym->pass == 0)
			{
			sym->pass = 1;

			// First use : output definition

			out_bit (1);
			out_bit (0);

			out_child_si (sym, ref_bit, sym->len, 1);

			sym->index = index_count++;
			}
		else
			{
			// Next use : output reference

			out_bit (1);
			out_bit (1);
			out_code (sym->index, ref_bit);
			ref_count++;
			}

		if (def_len) def_count++;
		}
	else
		{
		def_count = out_child_si (sym, ref_bit, def_len, def_count);
		}

	return def_count;
	}

static uchar out_child_si (symbol_t * sym, uchar_t ref_bit, uchar def_len, uchar def_count)
	{
	if (sym->size == 1)
		{
		// Output base code

		// Insert next flag inside a definition
		// No flag in a definition with a single base symbol
		if (def_len > 1) out_bit ((def_count == def_len) ? 0 : 1);

		out_bit (0);
		out_code (sym->code, 8);
		base_count++;

		if (def_len > 1) def_count++;
		}
	else
		{
		def_count = out_sym_si (sym->left,  ref_bit, def_len, def_count);
		def_count = out_sym_si (sym->right, ref_bit, def_len, def_count);
		}

	return def_count;
	}


static void compress_si ()
	{
	crunch_word ();

	if (opt_sym)
		{
		sym_sort (SORT_USE);
		sym_list (LIST_ALL);
		}

	// First consider that all the duplicated symbols are worth to keep
	// and compute the number of bits to reference all that symbols

	keep_count = keep_dup ();
	if (opt_verb) printf ("Duplicated symbols: %u\n\n", keep_count);
	uchar ref_bit = log2u (keep_count - 1);

	uchar best_bit = UCHAR_MAX;
	uint min_cost = UINT_MAX;

	// Iterate on the reference bits down to 0 to get the best one

	list_t * node;

	while (ref_bit > 0)
		{
		if (opt_verb) printf ("Reference bits: %u\n", ref_bit);

		// Compute the symbol costs

		keep_count = 0;

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			sym_cost_si (sym, ref_bit, 1);  // 1 = select
			node = node->next;
			}

		if (opt_verb) printf ("Worth symbols: %u\n", keep_count);

		// Discard worthless

		sym_sort (SORT_GAIN);

		uint keep_max = 1 << ref_bit;
		if (keep_count > keep_max)
			{
			for (uint i = keep_max; i < keep_count; i++)
				{
				index_sym_t * index = index_sym + i;
				symbol_t * sym = index->sym;
				sym->keep = 0;
				}

			keep_count = keep_max;
			}

		if (opt_verb) printf ("Kept symbols: %u\n", keep_count);

		// Recompute the symbol costs

		uint cost = 0;
		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			cost += sym_cost_si (sym, ref_bit, 0);  // 0 = no select
			node = node->next;
			}

		if (opt_verb) printf ("Total cost: %u bytes\n\n", cost / 8);

		// No need to go further when cost increases
		if (cost >= min_cost) break;

		min_cost = cost;
		best_bit = ref_bit;

		// Save the best selection

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			sym->best_keep = sym->keep;
			sym->best_len = sym->len;
			node = node->next;
			}

		ref_bit--;
		}

	if (opt_verb) printf ("Best bits: %u\n\n", best_bit);

	// Restore the best selection

	node = sym_root.next;
	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);
		sym->keep = sym->best_keep;
		sym->len = sym->best_len;
		node = node->next;
		}

	// Output the best selection

	out_pref_odd (best_bit - 1);

	node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = structof (position_t, node, node);
		out_sym_si (pos->sym, best_bit, 0, 0);  // 0 = currently outside a definition
		node = node->next;
		}

	out_pad ();
	}


// Decompression with "symbol"
// Embedded dictionary (internal)

static uint_t in_elem (uchar_t ref_bit)
	{
	uint_t size;

	if (!in_bit (1))  // byte code
		{
		uchar_t code = in_code (8);
		out_byte (code);
		size = 1;
		}
	else
		{
		if (in_bit (1))  // reference
			{
			uint_t i = in_code (ref_bit);
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

			// Iterate until next flag is false

			uchar count = 0;
			while (1)
				{
				uchar flag = in_bit (0);  // no shift - keep bit in input
				// No next flag in a definition with a single base symbol
				if (count || flag) in_bit (1);
				size += in_elem (ref_bit);
				if (!flag) break;  // was last symbol
				count++;
				}

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

	while (1)
		{
		// FIXME: not safe for small data
		// as remaining bits can be useful
		if (in_eof ()) break;
		in_elem (bit_len);
		}
	}


// Compression with "repeated symbol"
// Prepended dictionary (external)

static void compress_rse ()
	{
	crunch_word ();
	crunch_rep ();

	if (opt_sym)
		{
		sym_sort (SORT_ALL);
		sym_list (LIST_ALL);
		}

	// First consider that all the duplicated symbols are worth to keep
	// and compute the number of bits to reference all that symbols

	keep_count = keep_dup ();

	if (opt_verb) printf ("Duplicated symbols: %u\n\n", keep_count);
	uchar ref_bit = log2u (keep_count - 1);

	uchar best_bit = UCHAR_MAX;
	uint best_keep = UINT_MAX;
	uint min_cost = UINT_MAX;

	// Iterate on the reference bits down to 0 to get the best one

	list_t * node;

	while (ref_bit > 0)
		{
		if (opt_verb) printf ("Reference bits: %u\n", ref_bit);

		// Compute the symbol costs

		keep_count = 0;

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			if (!sym->repeat) sym_cost_rse (sym, ref_bit, 1);  // select
			node = node->next;
			}

		if (opt_verb) printf ("Worth symbols: %u\n", keep_count);

		// Discard worthless

		sym_sort (SORT_GAIN);

		uint keep_max = 1 << ref_bit;
		if (keep_count > keep_max)
			{
			for (uint i = keep_max; i < keep_count; i++)
				{
				index_sym_t * index = index_sym + i;
				symbol_t * sym = index->sym;
				sym->keep = 0;
				}

			keep_count = keep_max;
			}

		if (opt_verb) printf ("Kept symbols: %u\n", keep_count);

		// Recompute the symbol costs

		uint cost = 0;
		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			if (!sym->repeat) cost += sym_cost_rse (sym, ref_bit, 0);  // no select
			node = node->next;
			}

		if (opt_verb) printf ("Total cost: %u bytes\n\n", cost / 8);

		// No need to go further when cost increases
		if (cost >= min_cost) break;

		best_keep = keep_count;
		min_cost = cost;
		best_bit = ref_bit;

		// Save the best selection

		node = sym_root.next;
		while (node != &sym_root)
			{
			symbol_t * sym = structof (symbol_t, node, node);
			sym->best_keep = sym->keep;
			sym->best_len = sym->len;
			node = node->next;
			}

		ref_bit--;
		}

	if (opt_verb) printf ("Best bits: %u\n\n", best_bit);

	// Restore the best selection

	node = sym_root.next;
	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);
		sym->keep = sym->best_keep;
		sym->len = sym->best_len;
		node = node->next;
		}

	// Output the dictionary

	out_pref_odd (best_keep - 1);

	node = sym_root.next;
	while (node != &sym_root)
		{
		symbol_t * sym = structof (symbol_t, node, node);
		if (sym->keep)
			{
			out_child_se (sym, best_bit, sym->len, 1, 0);  // inside a definition
			sym->index = index_count++;
			}

		node = node->next;
		}

	node = pos_root.next;
	while (node != &pos_root)
		{
		position_t * pos = structof (position_t, node, node);
		symbol_t * sym = pos->sym;

		uint rep = sym->rep_count;
		if (sym->repeat)
			{
			// Repeated symbol
			sym = sym->left;

			// Can repeat only a base or a defined symbol
			if (sym->size == 1 || sym->keep)
				{
				out_bit (1);  // repeat
				out_bit (0);
				out_pref_odd (rep - 2);
				out_sym_se (sym, best_bit, 0, 0, 0);  // outside a definition - in repeat
				rep_count++;
				}
			else for (uint r = 0; r < rep; r++)
				out_sym_se (sym, best_bit, 0, 0, 1);  // outside a definition - at position
			}
		else
			{
			out_sym_se (sym, best_bit, 0, 0, 1);  // outside a definition - at position
			}

		node = node->next;
		}

	out_pad ();
	}


// Decompression with "repeated symbol"
// Prepended dictionary (external)

static void expand_rse ()
	{
	uint_t def_count = 1 + in_pref_odd ();
	uchar_t ref_bit = log2u (def_count - 1);

	// TODO: regroup the dictionary load with SE

	for (uint_t i = 0; i < def_count; i++)
		{
		elem_t * elem = elements + i;

		elem->base = patt_len;

		// Iterate until next flag is false

		uchar count = 0;
		while (1)
			{
			uchar flag = in_bit (0);  // no shift - keep bit in input
			// No next flag in a definition with a single base symbol
			if (count || flag) in_bit (1);

			if (in_bit (1))  // index
				patterns [patt_len++] = PATTERN_MAX | in_code (ref_bit);
			else
				patterns [patt_len++] = in_code (8);

			if (!flag) break;  // was last symbol
			count++;
			}

		elem->size = count + 1;
		}

	while (1)
		{
		if (in_eof ()) break;

		if (in_bit (1))
			{
			if (in_bit (1))
				{
				// stand alone index
				uint i = in_code (ref_bit);
				walk_elem (i);
				}
			else
				{
				// repeat
				uint_t rep = 2 + in_pref_odd ();
				if (in_bit (1))
					{
					// repeated index
					uint_t i = in_code (ref_bit);
					while (rep--) walk_elem (i);
					}
				else
					{
					// repeated base
					uchar_t code = in_code (8);
					while (rep--) out_byte (code);
					}
				}
			}
		else
			{
			// stand alone base
			uchar code = in_code (8);
			out_byte (code);
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
			opt = getopt (argc, argv, "cem:stv");
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
					else if (!strcmp (optarg, "rse"))
						opt_algo = ALGO_REP_SE;
					else
						error (1, 0, "unknown algorithm");

					break;

				case 's':  // list symbols
					opt_sym = 1;
					break;

				case 't':  // timing
					opt_time = 1;
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
			puts ("  -t  timing");
			puts ("  -v  verbose");
			puts ("");
			puts ("algorithms:");
			puts ("  b    base (no compression)");
			puts ("  rb   repeat base");
			puts ("  pb   prefixed base");
			puts ("  rpb  repeat prefixed base");
			puts ("  se   symbol external (prepended dictionary)");
			puts ("  si   symbol internal (embedded dictionary)");
			puts ("  rse  repeat symbol external (default)");
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
				printf ("Frame length: %u\n", size_in);
				printf ("Base symbol count: %u\n\n", sym_count);
				}

			if (opt_sym)
				{
				sym_sort (SORT_ALL);
				sym_list (LIST_ALL);
				}

			if (opt_verb) puts ("Compressing...\n");

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

				case ALGO_REP_SE:
					compress_rse ();
					break;

				default:
					compress_se ();
					break;

				}

			if (opt_verb)
				{
				puts ("FINAL");
				printf ("Frame length: %u\n", pos_count);
				printf ("Frame size: %u\n", size_out);

				printf ("Base count: %u\n", base_count);
				printf ("Ref count: %u\n", ref_count);
				printf ("Rep count: %u\n", rep_count);

				double ratio = (double) size_out / size_in;
				printf ("Compression ratio: %f\n\n", ratio);
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

				case ALGO_REP_SE:
					expand_rse ();
					break;

				default:
					expand_se ();
					break;

				}

			if (opt_verb) puts (" DONE\n");

			out_frame (argv [optind + 1]);
			break;
			}

		break;
		}

	clock_t clock_end = clock ();
	if (opt_time) printf ("elapsed=%lf msecs\n", (clock_end - clock_begin) * 1000.0 / CLOCKS_PER_SEC);

	return 0;
	}


//------------------------------------------------------------------------------
