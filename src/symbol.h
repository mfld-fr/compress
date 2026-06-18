//------------------------------------------------------------------------------
// Symbols
//------------------------------------------------------------------------------

#pragma once

#include "common.h"
#include "list.h"

// Symbol definitions

#define SYMBOL_MAX 65536  // 64K

struct symbol_s
	{
	list_t node;        // must be the first member

	uint_t pos_count;  // usage count in frame
	uint_t sym_count;  // usage count in tree
	uint_t rep_count;  // repetition count
	uint_t use_count;  // total usage count
	uint   rep_pos;    // repeated position count

	uchar   repeat; // repeat symbol
	uchar_t keep;   // define in the dictionary
	uint_t  index;  // index in the dictionary
	uint_t  len;    // definition length
	uint    pass;   // walk flag
	uint    cost;   // use cost
	uint    pcost;  // position cost (for RSE)
	int     gain;   // gain when defined

	uchar best_keep;  // save best selection
	uint  best_len;

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

extern uint keep_count;
extern uchar ref_bit;


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
	list_t node_hole;  // list of positions without pair

	uint_t base;

	symbol_t * sym;
	pair_t * pair;
	};

typedef struct position_s position_t;

extern list_t pos_root;
extern uint_t pos_count;  // list of positions


// Kind of sorting

#define SORT_ALL  0  // sort by impact (occurrences * size)
#define SORT_REP  1  // filter repeat out
#define SORT_USE  2  // sort by usage count
#define SORT_GAIN 3  // by gain
#define SORT_BASE 4  // by frame order
#define SORT_SIZE 5  // by size


struct index_sym_s
	{
	int key;
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

void sym_sort (uint_t kind);
void sym_list (uint_t filter);

void scan_base ();

void crunch_word ();
void crunch_rep ();

uint_t keep_dup ();

uint sym_cost_se (symbol_t * sym, uchar select);
uint sym_cost_si (symbol_t * sym, uchar select);
uint sym_cost_rse (symbol_t * sym, uchar select);

//------------------------------------------------------------------------------
