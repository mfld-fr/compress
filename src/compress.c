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

static symbol_t symbols [SYMBOL_MAX];
static uint_t sym_count;

static symbol_t * frame_sym [FRAME_MAX];
static uchar_t frame_mask [FRAME_MAX];
static uint_t frame_size;

static symbol_t pairs [PATTERN_MAX];
static uint_t pair_count;


// Symbols functions

static int sym_comp (const void * v1, const void * v2)
	{
	symbol_t * s1 = (symbol_t * ) v1;
	symbol_t * s2 = (symbol_t * ) v2;

	uint_t c1 = s1->count;
	uint_t c2 = s2->count;

	return (c1 < c2) ? 1 : ((c1 > c2) ? -1 : 0);
	}


static symbol_t * sym_add (uint_t base, uint_t size)
	{
	if (sym_count >= SYMBOL_MAX)
		error (1, 0, "too many symbols");

	symbol_t * s = &(symbols [sym_count++]);

	s->size = size;
	s->base = base;
	s->count = 1;

	return s;
	}


// Scan frame for all base symbols

static void scan_base ()
	{
	frame_size = frame_len;

	// Count symbol occurrences

	for (uint_t pos = 0; pos < frame_len; pos++)
		{
		symbol_t * s = &(symbols [(uint_t) frame_text [pos]]);

		if (!s->count)
			{
			s->base = pos;
			s->size = 1;
			}

		frame_sym [pos] = s;
		s->count++;
		}

	// Sort symbols by count

	qsort (symbols, BASE_MAX, sizeof (symbol_t), sym_comp);

	// Display symbol statistics

	for (uint_t i = 0; i < BASE_MAX; i++)
		{
		symbol_t * s = &(symbols [i]);

		if (!s->count) break;

		printf ("symbol: base=%5x code=%2x count=%5u\n",
			s->base, frame_text [s->base], s->count);

		sym_count++;
		}

	printf ("\nsymbol count=%u\n", sym_count);

	// Compute entropy

	double entropy = 0.0;

	for (uint_t i = 0; i < sym_count; i++)
		{
		symbol_t * s = &(symbols [i]);

		double p = (double) s->count / frame_len;
		entropy += -p * log2 (p);
		}

	printf ("entropy=%f\n\n", entropy);
	}


static symbol_t * pair_add (uint_t base, uint_t size)
	{
	if (pair_count >= PATTERN_MAX)
		error (1, 0, "too many patterns");

	symbol_t * s = &(pairs [pair_count++]);

	s->base = base;
	s->size = size;
	s->count = 1;

	return s;
	}


// Scan frame for all pairs

static void scan_pair ()
	{
	// Frame mask is used to optimize the scan

	memset (frame_mask , 0, sizeof frame_mask);

	memset (pairs , 0, sizeof pairs);
	pair_count = 0;

	for (uint_t left = 0; left <= frame_size - 3; left++)
		{
		if (!frame_mask [left])  // skip already found pair
			{
			frame_mask [left] = 1;  // pair now found there

			// Add new pair

			symbol_t * p = pair_add (left, 2);

			p->left = frame_sym [left];
			p->right = frame_sym [left + 1];

			// Scan for pair duplicates

			for (uint_t right = left + 1; right <= frame_size - 2; right++)
				{
				if (!frame_mask [right] &&  // skip already found pair
					(frame_sym [left] == frame_sym [right]) &&
					(frame_sym [left+1] == frame_sym [right + 1]))
					{
					frame_mask [right] = 1;  // pair now found there

					p->count++;
					}
				}
			}
		}

	// Sort pairs by count

	qsort (pairs, pair_count, sizeof (symbol_t), sym_comp);
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

		printf ("frame length=%u\n\n", frame_len);

		memset (symbols, 0, sizeof symbols);
		sym_count = 0;

		scan_base ();
		sym_count = BASE_MAX;

		if (frame_len < 3)
			error (1, 0, "frame too short");

		scan_pair ();

		// Display pair statistics

		for (uint_t i = 0; i < pair_count; i++)
			{
			symbol_t * p = &(pairs [i]);

			printf ("pattern: base=%5x left=%2x right=%2x count=%5u \n",
				p->base, frame_text [p->base], frame_text [p->base + 1], p->count);

			}

		printf ("\npattern count=%5u\n", pair_count);
		break;
		}

	f ? fclose (f) : 0;

	clock_t clock_end = clock ();
	printf ("elapsed=%lu usecs\n", (clock_end - clock_begin));

	return 0;
	}
