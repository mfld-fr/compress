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
static uchar_t frame_mask [FRAME_MAX];
static uint_t frame_len;


// Symbol definitions

struct symbol_s
	{
	uint_t base;
	uint_t size;
	uint_t count;
	uint_t left;
	uint_t right;
	};

typedef struct symbol_s symbol_t;

#define BASE_MAX 256
#define SYMBOL_MAX 65536  // 64K

static symbol_t symbols [SYMBOL_MAX];
static uint_t base_count;
static uint_t sym_count;


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
	// Count symbol occurrences

	for (uint_t pos = 0; pos < frame_len; pos++)
		{
		symbol_t * s = &(symbols [(uint_t) frame_text [pos]]);

		if (!s->count)
			{
			s->base = pos;
			s->size = 1;
			}

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

		base_count++;
		}

	printf ("\nsymbol count=%u\n", base_count);

	// Compute entropy

	double entropy = 0.0;

	for (uint_t i = 0; i < base_count; i++)
		{
		symbol_t * s = &(symbols [i]);

		double p = (double) s->count / frame_len;
		entropy += -p * log2 (p);
		}

	printf ("entropy=%f\n\n", entropy);
	}


// Scan frame for all pairs

static void scan_pair ()
	{
	// Reset the frame mask
	// used to optimize the scan

	memset (frame_mask, 0, sizeof frame_mask);

	for (uint_t base = 0; base <= frame_len - 3; base++)
		{
		if (!frame_mask [base])  // skip already found pair
			{
			// Add new pair

			symbol_t * p = sym_add (base, 2);
			frame_mask [base] = 1;  // pair now found there

			for (uint_t off = base + 1; off <= frame_len - 2; off++)
				{
				if (!frame_mask [off] &&  // skip already found pair
					(frame_text [base] == frame_text [off]) &&
					(frame_text [base+1] == frame_text [off + 1]))
					{
					frame_mask [off] = 1;  // pair now found there
					p->count++;
					}
				}
			}
		}

	// Sort pairs by count

	qsort (symbols + BASE_MAX, sym_count - BASE_MAX, sizeof (symbol_t), sym_comp);

	// Display pair statistics

	for (uint_t i = BASE_MAX; i < sym_count; i++)
		{
		symbol_t * p = &(symbols [i]);

		printf ("pattern: base=%5x left=%2x right=%2x count=%5u \n",
			p->base, frame_text [p->base], frame_text [p->base + 1], p->count);

		}

	putchar ('\n');

	printf ("pattern count=%5u\n", sym_count - BASE_MAX);
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
		base_count = 0;
		sym_count = 0;

		scan_base ();
		sym_count = BASE_MAX;

		if (frame_len < 3)
			error (1, 0, "frame too short");

		scan_pair ();

		break;
		}

	f ? fclose (f) : 0;

	clock_t clock_end = clock ();
	printf ("elapsed=%lu usecs\n", (clock_end - clock_begin));

	return 0;
	}
