
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <error.h>


typedef unsigned int uint_t;


// Frame to scan

#define FRAME_MAX 256

static char frame [FRAME_MAX];
static char frame_mask [FRAME_MAX];


// Pattern definitions

struct pattern_s
	{
	uint_t size;
	uint_t base;
	uint_t count;
	};

typedef struct pattern_s pattern_t;

#define PATTERN_MAX 256

static pattern_t patterns [PATTERN_MAX];
static uint_t pcount;


// Add pattern

static pattern_t * pattern_add (uint_t size, uint_t base)
	{
	if (pcount >= PATTERN_MAX)
		error (1, 0, "out of pattern space");

	pattern_t * p = &(patterns [pcount++]);

	p->size = size;
	p->base = base;
	p->count = 1;

	return p;
	}


// Scan frame for one pattern size

static void level_scan (char * frame, uint_t flen, uint_t psize)
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
				if (!frame_mask [off] && !memcmp (frame + base, frame + off, psize))
					{
					printf ("match: base=%u off=%u\n", base, off);

					if (!p) p = pattern_add (psize, base);
					p->count++;

					frame_mask [off] = 1;  // pattern already found there
					off += psize - 1;  // skip end of found pattern
					}
				}
			}
		}
	}


// Scan frame for all pattern sizes

static void frame_scan (char * frame, uint_t flen)
	{
	uint_t psize;
	uint_t pmax = flen >> 1;

	// Reset found patterns

	memset (patterns, 0, sizeof patterns);
	pcount = 0;

	// Scan for patterns

	for (psize = 1; psize <= pmax; psize++)
		{
		printf ("pattern size=%u\n", psize);
		level_scan (frame, flen, psize);
		putchar ('\n');
		}

	// Display pattern statistics

	printf ("pattern count=%u\n", pcount);

	for (uint_t i = 0; i < pcount; i++)
		{
		pattern_t * p = &(patterns [i]);
		printf ("pattern: size=%u base=%u count=%u save=%u\n",
			p->size, p->base, p->count, (p->count - 1) * p->size);
		}

	putchar ('\n');
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

		uint_t flen = 0;

		while (1)
			{
			int c = fgetc (f);
			if (c < 0 || c > 255) break;

			if (flen >= FRAME_MAX)
				error (1, 0, "frame too long");

			frame [flen++] = (char) c;
			}

		printf ("frame length=%u\n", flen);
		putchar ('\n');

		frame_scan (frame, flen);
		break;
		}

	f ? fclose (f) : 0;

	clock_t clock_end = clock ();
	printf ("elapsed=%lu\n", (clock_end - clock_begin));

	return 0;
	}
