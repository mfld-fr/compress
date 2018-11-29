//------------------------------------------------------------------------------
// Bit stream
//------------------------------------------------------------------------------

#include "stream.h"

#include <error.h>
#include <errno.h>


// Global data

uchar_t frame_in [FRAME_MAX];
uchar_t frame_out [FRAME_MAX];

uint_t size_in;
uint_t size_out;


// Local data

static uint_t pos_in;

static uchar_t byte_in;
static uchar_t byte_out;

static uchar_t shift_in;
static uchar_t shift_out;


// Frame load & store

void in_frame (const char * name)
	{
	FILE * file = fopen (name, "r");
	if (!file) error (1, errno, "open failed");

	size_t size = fread (frame_in, sizeof (uchar_t), FRAME_MAX, file);
	if (ferror (file)) error (1, errno, "load failed");

	size_in = size;
	fclose (file);
	}


void out_frame (const char * name)
	{
	FILE * file = fopen (name, "w");
	if (!file) error (1, errno, "open failed");

	size_t size = fwrite (frame_out, sizeof (uchar_t), size_out, file);
	if ((size != size_out) || ferror (file)) error (1, errno, "store failed");

	fclose (file);
	}


// Byte code

void out_byte (uchar_t val)
	{
	if (size_out >= FRAME_MAX)
		error (1, 0, "out overflow");

	frame_out [size_out++] = val;
	}


uchar_t in_byte ()
	{
	if (pos_in >= FRAME_MAX)
		error (1, 0, "in overflow");

	return frame_in [pos_in++];
	}


// Bit code

void out_bit (uchar_t val)
	{
	byte_out = byte_out | (val ? 128 : 0);  // 2^(8-1)
	if (++shift_out == 8)
		{
		out_byte (byte_out);

		shift_out = 0;
		byte_out = 0;
		return;
		}

	byte_out >>= 1;
	}


uchar_t in_bit ()
	{
	if (!shift_in)
		{
		byte_in = in_byte ();
		shift_in = 8;
		}

	uchar_t val = byte_in & 1;
	byte_in >>= 1;
	shift_in--;

	return val;
	}


void out_pad ()
	{
	if (shift_out > 0)
		{
		byte_out >>= (7 - shift_out);

		if (size_out >= FRAME_MAX)
			error (1, 0, "out overflow");

		frame_out [size_out++] = byte_out;

		shift_out = 0;
		byte_out = 0;
		}
	}


// Basic code

void out_code (uint_t code, uchar_t len)
	{
	if (len > 16)
		error (1, 0, "code too long");

	uint_t i = 0;

	while (1)
		{
		out_bit (code & 1);
		if (++i == len) break;
		code >>= 1;
		}
	}


uint_t in_code (uchar_t len)
	{
	if (len > 16)
		error (1, 0, "code too long");

	uint_t code = 0;
	uint_t i = 0;

	while (1)
		{
		code |= in_bit () ? 32768 : 0;  // 2^(16-1)
		if (++i == len) break;
		code >>= 1;
		}

	code >>= (16 - len);
	return code;
	}


// Prefixed code

void out_len (uchar_t len)
	{
	while (len-- > 0) out_bit (1);
	out_bit (0);
	}

uchar_t in_len ()
	{
	uchar_t len = 0;

	while (in_bit ()) len++;

	return len;
	}


void out_pref_odd (uint_t val)
	{
	uchar_t prefix = 0;
	uint_t base0 = 0;
	uint_t base1 = 1;

	while (val >= base1)
		{
		base0 = base1;
		base1 = (base1 << 1) | 1;
		prefix++;
		}

	out_len (prefix);

	if (prefix) out_code (val - base0, prefix);
	}


uint_t in_pref_odd ()
	{
	uchar_t suffix = 0;
	uint_t base = 0;

	while (in_bit ())
		{
		suffix++;
		base = (base << 1) | 1;
		}

	uint_t val = 0;
	if (suffix) val = base + in_code (suffix);
	return val;
	}


void out_pref_even (uint_t val)
	{
	uchar_t prefix = 0;
	uint_t base0 = 0;
	uint_t base1 = 2;

	while (val >= base1)
		{
		base0 = base1;
		base1 = (base1 << 1) | 2;
		prefix++;
		}

	out_len (prefix);

	out_code (val - base0, 1 + prefix);
	}


uint_t in_pref_even ()
	{
	uchar_t suffix = 0;
	uint_t base = 0;

	while (in_bit ())
		{
		suffix++;
		base = (base << 1) | 2;
		}

	return  (base + in_code (1 + suffix));
	}


uchar_t log2u (uint_t val)
	{
	uchar_t log = 1;
	while (val >>= 1) log++;
	return log;
	}


//------------------------------------------------------------------------------
