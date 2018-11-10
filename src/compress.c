
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <time.h>


typedef unsigned int uint_t;


// Frame to scan

#define FRAME_MAX 256

static char level_mask [FRAME_MAX];


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


// Scan frame for one pattern size

static void level_scan (char * frame, uint_t flen, uint_t psize)
	{
	uint_t rend = flen - psize;
	uint_t lend = rend - psize;

	uint_t base;
	uint_t off;

	// Reset the level mask
	// used to optimize the scan

	memset (level_mask, 0, sizeof level_mask);

	for (base = 0; base <= lend; base++)
		{
		// Skip already found pattern

		if (!level_mask [base])
			{
			level_mask [base] = 1;

			pattern_t * p = NULL;

			uint_t lbeg = base + psize;

			for (off = lbeg; off <= rend; off++)
				{
				if (!level_mask [off] && !memcmp (frame + base, frame + off, psize))
					{
					printf ("match: base=%u off=%u\n", base, off);

					if (!p)
						{
						// Create new pattern

						p = &(patterns [pcount++]);

						p->size = psize;
						p->base = base;
						p->count = 1;
						}

					p->count++;

					level_mask [off] = 1;  // pattern already found there
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

	char * frame = "abccccabcc";
	uint_t flen = strlen (frame);

	frame_scan (frame, flen);

	/*
	FILE * f = NULL;

	while (1)
		{
		memset (&_nodes, 0, sizeof _nodes);
		memset (&_frame, 0, sizeof _frame);

		f = fopen (argv [1], "r");
		if (!f) break;

		while (1)
			{
			int r = fgetc (f);
			if (r < 0 || r > 255) break;

			word_t n = (word_t) r;
			if (n >= NODE_MAX)
				{
				puts ("fatal: too many nodes");
				abort ();
				}

			Node_t * node = _nodes + n;
			if (!node->used)
				{
				node->used = 1;
				node->repeat = 1;
				node->length = 1;
				node->pattern [0] = n;
				node->use_count = 1;

				if (n >= _node_last) _node_last = n + 1;
				_node_count++;
				}
			else
				{
				node->use_count++;
				}

			if (_frame_len >= _FRAME_MAX)
				{
				puts ("fatal: frame too long");
				abort ();
				}

			_frame [_frame_len++] = n;
			}

		puts ("initial:");
		printf ("  length: %u\n", _frame_len);
		printf ("  count:  %u\n", _node_count);
		printf ("  last:   %u\n", _node_last);
		putchar ('\n');

		while (1)
			{
			printf ("pass %u:\n", ++_pass_count);

			int res = analyze ();
			if (res < 0)
				{
				puts ("fatal: analyze error");
				abort ();
				}

			putchar ('\n');
			printf ("  frame: %u\n", _frame_len);
			printf ("  count: %u\n", _node_count);
			printf ("  last:  %u\n", _node_last);
			putchar ('\n');

			if (!res)
				{
				puts ("  no more pass");
				putchar ('\n');
				break;
				}
			}

		break;
		}

	f ? fclose (f) : 0;
	*/

	clock_t clock_end = clock ();
	printf ("elapsed=%lu\n", (clock_end - clock_begin));

	return 0;
	}
