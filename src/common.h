#pragma once

typedef unsigned char uchar_t;
typedef unsigned char uchar;
typedef unsigned int uint_t;
typedef unsigned int uint;

#define structof (type, member, pointer) ( \
	(type *) ((char *) pointer - offsetof (type, member)))
