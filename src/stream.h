//------------------------------------------------------------------------------
// Bit stream
//------------------------------------------------------------------------------

#pragma once

#include <stdio.h>

#include "common.h"


#define CODE_MAX 256  // 8 bits
#define FRAME_MAX 65536  // 64K


extern uchar_t frame_in [FRAME_MAX];
extern uchar_t frame_out [FRAME_MAX];

extern uint_t size_in;
extern uint_t size_out;


void in_frame ();
void out_frame ();

void out_byte (uchar_t val);
uchar_t in_byte ();

void out_bit (uchar_t val);
uchar_t in_bit ();

void out_pad ();

void out_code (uint_t code, uchar_t len);
uint_t in_code (uchar_t len);

void out_prefix (uint_t val);
uint_t in_prefix ();

//------------------------------------------------------------------------------
