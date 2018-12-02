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

	uchar_t code;  // base byte code (primary symbol)
	uint_t index;  // index after sort
	uint_t rep;    // repeat counter

	uint_t base;  // offset of first occurrence in input frame
	uint_t size;  // size in input codes

	// For node symbol (secondary)

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct symbol_s symbol_t;

extern list_t sym_root;
extern uint_t sym_count;


// Position definitions

struct pair_s;

struct position_s
	{
	list_t node;  // must be the first member

	uint_t base;

	symbol_t * sym;
	struct pair_s * pair;
	};

typedef struct position_s position_t;

extern list_t pos_root;
extern uint_t pos_count;


// Pair definitions

struct pair_s
	{
	list_t node;  // must be the first member

	uint_t count;  // number of occurrences in the frame

	struct symbol_s * left;
	struct symbol_s * right;
	};

typedef struct pair_s pair_t;


// Indexes for quick sort

#define SORT_ALL 0  // all symbols
#define SORT_REP 1  // filter repeat out
#define SORT_DUP 2  // duplicated only

struct index_sym_s
	{
	uint_t key;
	symbol_t * sym;
	};

typedef struct index_sym_s index_sym_t;

extern index_sym_t index_sym [SYMBOL_MAX];


// Global functions

symbol_t * sym_add ();
uint_t sym_sort (uint_t filter);
void sym_list ();

void scan_base ();

void crunch_word ();
void crunch_rep ();


//------------------------------------------------------------------------------
