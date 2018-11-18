//------------------------------------------------------------------------------
// Compressor
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <error.h>
#include <math.h>


typedef unsigned char uchar_t;
typedef unsigned int uint_t;


// Frame to scan

#define FRAME_MAX 65536  // 64K

static uchar_t frame_text [FRAME_MAX];
static uint_t frame_len;


// Symbol definitions

struct symbol_s
	{
	uint_t base;
	uint_t size;
	uint_t count;

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct symbol_s symbol_t;

#define BASE_MAX 256
#define SYMBOL_MAX 65536  // 64K
#define PATTERN_MAX (SYMBOL_MAX - BASE_MAX)

// TODO: move symbols from static to dynamic

static symbol_t symbols [SYMBOL_MAX];
static uint_t sym_count;

static symbol_t * frame_sym [FRAME_MAX];
static uint_t frame_size;

static symbol_t * frame_pair [FRAME_MAX];
static symbol_t pairs [PATTERN_MAX];
static uint_t pair_count;


// TODO: merge index and dynamic table

struct key_sym_s
	{
	uint_t index;
	symbol_t * sym;
	};

typedef struct key_sym_s key_sym_t;

static key_sym_t keys [SYMBOL_MAX];


static int key_comp (const void * v1, const void * v2)
	{
	key_sym_t * k1 = (key_sym_t * ) v1;
	key_sym_t * k2 = (key_sym_t * ) v2;

	uint_t i1 = k1->index;
	uint_t i2 = k2->index;

	return (i1 < i2) ? 1 : ((i1 > i2) ? -1 : 0);
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
		key_sym_t * key = keys + i;
		symbol_t * s = symbols + i;

		key->index = s->count;
		key->sym = s;
		}

	qsort (keys, sym_count, sizeof (key_sym_t), key_comp);

	// List the used symbols

	uint_t count = 0;
	double entropy = 0.0;

	for (uint_t i = 0; i < sym_count; i++)
		{
		key_sym_t * k = keys + i;
		symbol_t * s = k->sym;

		if (!s->count) break;

		count++;

		double p = (double) s->count / frame_size;
		entropy += -p * log2 (p);

		printf ("symbol [%u]: ", i);

		if (s->size == 1)
			printf ("code=%hu ", frame_text [s->base]);
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
	// Count symbol occurrences

	for (uint_t pos = 0; pos < frame_len; pos++)
		{
		symbol_t * s = symbols + (uint_t) frame_text [pos];

		if (!s->count)
			{
			s->base = pos;
			s->size = 1;
			}

		frame_sym [pos] = s;
		s->count++;
		}

	sym_count = BASE_MAX;
	frame_size = frame_len;
	}


static symbol_t * pair_add (uint_t base, uint_t size)
	{
	if (pair_count >= PATTERN_MAX)
		error (1, 0, "too many patterns");

	symbol_t * s = pairs +  pair_count++;

	s->base = base;
	s->size = size;
	s->count = 1;

	return s;
	}


// Scan frame for all pairs

static void scan_pair ()
	{
	// Frame mask is used to optimize the scan

	memset (frame_pair , 0, sizeof frame_pair);

	memset (pairs , 0, sizeof pairs);
	pair_count = 0;

	for (uint_t left = 0; left <= frame_size - 3; left++)
		{
		if (!frame_pair [left])  // skip already found pair
			{
			// Add new pair

			symbol_t * p = pair_add (left, 2);

			p->left = frame_sym [left];
			p->right = frame_sym [left + 1];

			frame_pair [left] = p;  // pair now found there

			// Scan for pair duplicates

			for (uint_t right = left + 1; right <= frame_size - 2; right++)
				{
				if (!frame_pair [right] &&  // skip already found pair
					(frame_sym [left] == frame_sym [right]) &&
					(frame_sym [left+1] == frame_sym [right + 1]))
					{
					frame_pair [right] = p;  // pair now found there

					p->count++;
					}
				}

			// Do not count the symmetric pairs
			// TODO: ugly way to do - better to reject symmetric pairs

			if (p->left == p->right) p->count = 0;
			}
		}
	}


static int shrink_pair ()
	{
	int shrink = 0;  // no shrink

	// Build pair index and sort by occurrences

	for (uint_t i = 0; i < pair_count; i++)
		{
		key_sym_t * key = keys + i;
		symbol_t * p = pairs + i;

		key->index = p->count;
		key->sym = p;
		}

	qsort (keys, pair_count, sizeof (key_sym_t), key_comp);

	// Try to shrink all repeated pairs

	for (uint_t i = 0; i < pair_count; i++)
		{
		key_sym_t * k = keys + i;
		symbol_t * p = k->sym;

		// Skip the symmetric pairs

		if (p->left == p->right) continue;

		// Skip any singleton or invalid pairs

		if (p->count < 2) continue;

		// Check all pair occurrences

		symbol_t * s = NULL;

		for (uint_t pos = p->base; pos < frame_size - 1; pos++)
			{
			if (frame_pair [pos] != p) continue;

			symbol_t * p_left = NULL;
			uint_t c_left = 0;

			if (pos > 0)
				{
				p_left = frame_pair [pos - 1];
				c_left = p_left->count;
				}

			symbol_t * p_right = NULL;
			uint_t c_right = 0;
			if (pos < frame_size - 2)
				{
				p_right = frame_pair [pos + 1];
				c_right = p_right->count;
				}

			// Can shrink if stronger than neighbors

			if ((c_left <= p->count) && (p->count >= c_right))
				{
				// Update child counts

				p->left->count--;
				p->right->count--;

				// Create a new symbol from the pair

				if (!s)
					{
					s = sym_add (pos, 2);

					s->left = p->left;
					s->right = p->right;
					}

				s->count++;

				// Invalidate neighbors

				if (p_left) p_left->count = 0;
				if (p_right) p_right->count = 0;

				// Replace current pair by the new symbol

				frame_sym [pos] = s;

				// Shift the frame left

				uint_t shift = (frame_size - pos - 2) * sizeof (symbol_t *);
				memcpy (frame_sym + pos + 1, frame_sym + pos + 2, shift);
				memcpy (frame_pair + pos + 1, frame_pair + pos + 2, shift);

				frame_size--;

				shrink = 1;
				}
			}
		}

	return shrink;
	}


int main (int argc, char * argv [])
	{
	clock_t clock_begin = clock ();

	FILE * f = NULL;

	while (1)
		{
		if (argc < 2)
			error (1, 0, "missing input file as argument");

		f = fopen (argv [1], "r");
		if (!f) break;

		while (1)
			{
			int c = fgetc (f);
			if (c < 0 || c > 255) break;

			if (frame_len >= FRAME_MAX)
				error (1, 0, "frame too long");

			frame_text [frame_len++] = c;
			}

		puts ("INITIAL:\n");
		printf ("frame length=%u\n\n", frame_len);

		memset (symbols, 0, sizeof (symbols));
		sym_count = 0;

		scan_base ();
		sym_list ();

		if (frame_len < 3)
			error (1, 0, "frame too short");

		// Iterate of pair scan and shrink

		while (1)
			{
			scan_pair ();
			if (!shrink_pair ()) break;
			}

		puts ("FINAL:\n");
		printf ("frame length=%u\n\n", frame_size);
		sym_list ();

		double ratio = (double) frame_size / frame_len;
		printf ("ratio=%f\n", ratio);

		break;
		}

	f ? fclose (f) : 0;

	clock_t clock_end = clock ();
	printf ("elapsed=%lu usecs\n", (clock_end - clock_begin));

	return 0;
	}
