
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
static uint_t flen;


// Symbol definitions

struct symbol_s
	{
	uint_t base;
	uint_t count;
	};

typedef struct symbol_s symbol_t;

#define SYMBOL_MAX 256

static symbol_t symbols [SYMBOL_MAX];


// Pattern definitions

struct pattern_s
	{
	uint_t size;
	uint_t base;
	uint_t count;
	};

typedef struct pattern_s pattern_t;

#define LENGTH_MAX 2
#define PATTERN_MAX 65536  // 64K

static pattern_t patterns [PATTERN_MAX];
static uint_t pcount;


// Symbols functions

static int symbol_comp (const void * v1, const void * v2)
	{
	symbol_t * s1 = (symbol_t * ) v1;
	symbol_t * s2 = (symbol_t * ) v2;

	uint_t c1 = s1->count;
	uint_t c2 = s2->count;

	return (c1 < c2) ? 1 : ((c1 > c2) ? -1 : 0);
	}


// Scan frame for all symbols

static void scan_symbol ()
	{
	// Reset symbols

	memset (symbols, 0, sizeof symbols);

	// Count symbol occurrences

	for (uint_t pos = 0; pos < flen; pos++)
		{
		symbol_t * s = &(symbols [(uint_t) frame_text [pos]]);

		if (!s->count) s->base = pos;
		s->count++;
		}

	// Sort symbols by count

	qsort (symbols, SYMBOL_MAX, sizeof (symbol_t), symbol_comp);

	// Display symbol statistics

	uint_t scount = 0;

	for (uint_t i = 0; i < SYMBOL_MAX; i++)
		{
		symbol_t * s = &(symbols [i]);

		if (!s->count) break;

		printf ("symbol: base=%5u code=%3hhu count=%5u\n",
			s->base, frame_text [s->base], s->count);

		scount++;
		}

	printf ("\nsymbol count=%u\n", scount);

	// Compute entropy

	double entropy = 0.0;

	for (uint_t i = 0; i < scount; i++)
		{
		symbol_t * s = &(symbols [i]);

		double p = (double) s->count / flen;
		entropy += -p * log2 (p);
		}

	printf ("entropy=%f\n\n", entropy);
	}


// Pattern function

static int pattern_compare (const void * v1, const void * v2)
	{
	pattern_t * p1 = (pattern_t * ) v1;
	pattern_t * p2 = (pattern_t * ) v2;

	uint_t c1 = p1->count;
	uint_t c2 = p2->count;

	return (c1 < c2) ? 1 : ((c1 > c2) ? -1 : 0);
	}


static pattern_t * pattern_add (uint_t size, uint_t base)
	{
	if (pcount >= PATTERN_MAX)
		error (1, 0, "too many patterns");

	pattern_t * p = &(patterns [pcount++]);

	p->size = size;
	p->base = base;
	p->count = 1;

	//printf ("pattern count=%u\n", pcount);

	return p;
	}


// Scan frame for one pattern size

static void level_scan (uint_t psize)
	{
	uint_t rend = flen - psize;
	uint_t lend = rend - psize;

	uint_t base;
	uint_t off;

	// Reset the level mask
	// used to optimize the scan

	memset (frame_mask, 0, sizeof frame_mask);

	for (base = 0; base <= lend; base++)
		{
		// Skip already found pattern

		if (!frame_mask [base])
			{
			frame_mask [base] = 1;

			pattern_t * p = NULL;

			uint_t lbeg = base + psize;

			for (off = lbeg; off <= rend; off++)
				{
				if (!frame_mask [off] && !memcmp (frame_text + base, frame_text + off, psize))
					{
					//printf ("match: base=%u off=%u\n", base, off);

					if (!p) p = pattern_add (psize, base);
					p->count++;

					frame_mask [off] = 1;  // pattern already found there
					//off += psize - 1;  // skip end of found pattern
					}
				}
			}
		}
	}


// Scan frame for all patterns

static void scan_pattern ()
	{
	uint_t pmax = flen >> 1;
	pmax = (pmax > LENGTH_MAX) ? LENGTH_MAX : pmax;

	// Reset found patterns

	memset (patterns, 0, sizeof patterns);
	pcount = 0;

	// Scan for patterns

	for (uint_t psize = 2; psize <= pmax; psize++)
		{
		//printf ("pattern size=%u\n", psize);
		level_scan (psize);
		//putchar ('\n');
		}

	// Sort patterns by count

	qsort (patterns, pcount, sizeof (pattern_t), pattern_compare);

	// Display pattern statistics

	for (uint_t i = 0; i < pcount; i++)
		{
		pattern_t * p = &(patterns [i]);

		printf ("pattern: base=%5u code1=%3hhu code2=%3hhu count=%5u \n",
			p->base, frame_text [p->base], frame_text [p->base + 1], p->count);

		}

	putchar ('\n');

	printf ("pattern count=%u\n", pcount);
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

			if (flen >= FRAME_MAX)
				error (1, 0, "frame too long");

			frame_text [flen++] = c;
			}

		printf ("frame length=%u\n\n", flen);

		scan_symbol ();

		scan_pattern ();

		break;
		}

	f ? fclose (f) : 0;

	clock_t clock_end = clock ();
	printf ("elapsed=%lu usec\n", (clock_end - clock_begin));

	return 0;
	}
