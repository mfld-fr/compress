//------------------------------------------------------------------------------
// Bit stream
//------------------------------------------------------------------------------

#pragma once

#include <stdio.h>

#include "common.h"


#define CODE_MAX 256  // 8 bits
#define FRAME_MAX 65536  // 64K


// Global data

extern uchar_t frame_in [FRAME_MAX];
extern uchar_t frame_out [FRAME_MAX];

extern uint_t size_in;
extern uint_t size_out;


// Global functions

void in_frame ();
void out_frame ();

void out_byte (uchar_t val);
uchar in_eof ();
uchar_t in_byte ();

void out_bit (uchar_t val);
uchar_t in_bit ();

void out_pad ();

void out_code (uint_t code, uchar_t len);
uint_t in_code (uchar_t len);

void out_len (uchar_t val);
uchar_t in_len ();

uint cost_pref_odd (uint val);
void out_pref_odd (uint_t val);
uint_t in_pref_odd ();

void out_pref_even (uint_t val);
uint_t in_pref_even ();

uchar_t log2u (uint_t val);


//------------------------------------------------------------------------------
