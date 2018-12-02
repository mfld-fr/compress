#pragma once

typedef unsigned char uchar_t;
typedef unsigned int uint_t;

#define structof (type, member, pointer) ( \
	(type *) ((char *) pointer - offsetof (type, member)))
