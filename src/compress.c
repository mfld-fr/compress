
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#define FRAME_MAX 256

typedef unsigned char  byte_t;
typedef unsigned short word_t;


static void sweep (byte_t * frame, word_t flen, word_t wsize)
	{
	word_t rend = flen - wsize;
	word_t lend = rend - wsize;

	word_t base;
	word_t off;

	for (base = 0; base <= lend; base++)
		{
		word_t lbeg = base + wsize;

		for (off = lbeg; off <= rend; off++)
			{
			if (!memcmp (frame + base, frame + off, wsize))
				{
				printf ("match: base=%u off=%u\n", base, off);
				}
			}
		}
	}


static void scan (byte_t * frame, word_t flen)
	{
	word_t wsize;

	for (wsize = 1; wsize <= (flen >> 1); wsize++)
		{
		printf ("window size=%u\n", wsize);
		sweep (frame, flen, wsize);
		putchar ('\n');
		}
	}


int main (int argc, char * argv [])
	{
	byte_t * frame = (byte_t *) "abcdefabc";
	word_t flen = 9;

	scan (frame, flen);

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

	return 0;
	}
