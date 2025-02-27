//------------------------------------------------------------------------------
// Symbols
//------------------------------------------------------------------------------

#pragma once


// Symbol definitions

#define SYMBOL_MAX 65536  // 64K

struct symbol_s
	{
	list_t node;  // must be the first member

	uint_t pos_count;  // number of occurrences in the frame
	uint_t sym_count;  // number of occurrences in the tree
	uint_t rep_count;  // number of repetitions (1 = repeated)
	uint_t dup_count;  // total number of occurences (= pos_count + tree_count)

	uchar_t keep;   // define this symbol in the dictionary
	uint_t  index;  // index in the dictionary
	uint_t  len;    // length of symbol when defined or used
	uint    pass;   // pass number that discarded this symbol

	int tree_gain;  // gain in tree when defined
	int pos_gain;   // gain in frame when defined
	int all_gain;   // overall gain when defined

	uchar_t code;  // byte code of base symbol
	uint_t  base;  // offset of first occurrence in input frame
	uint_t  size;  // size in byte codes

	// For secondary symbol

	struct symbol_s * left;   // left or repeated child
	struct symbol_s * right;  // right child
	};

typedef struct symbol_s symbol_t;

extern list_t sym_root;
extern uint_t sym_count;


// Pair definitions

struct pair_s
	{
	list_t node;  // must be the first member

	uint_t count;  // number of occurrences in the frame

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct pair_s pair_t;


// Position definitions

struct position_s
	{
	list_t node;  // must be the first member

	uint_t base;

	symbol_t * sym;
	pair_t * pair;
	};

typedef struct position_s position_t;

extern list_t pos_root;
extern uint_t pos_count;


// Hole definitions

struct hole_s
	{
	list_t node;  // must be the first member

	position_t * pos;
	};

typedef struct hole_s hole_t;


// Kind of sorting

#define SORT_ALL 0  // sort by impact (occurences * size)
#define SORT_REP 1  // filter repeat out
#define SORT_DUP 2  // sort by occurences

struct index_sym_s
	{
	uint_t key;
	symbol_t * sym;
	};

typedef struct index_sym_s index_sym_t;

extern index_sym_t index_sym [SYMBOL_MAX];
extern uint_t index_count;


// Listing filters

#define LIST_ALL  0  // list all symbols
#define LIST_KEEP 1  // list only defined


// Global functions

symbol_t * sym_add ();
uint_t sym_sort (uint_t kind);
void sym_list (uint_t filter);

uint_t filter_init ();

void scan_base ();

void crunch_word ();
void crunch_rep ();


//------------------------------------------------------------------------------
