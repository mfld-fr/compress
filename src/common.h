#pragma once

typedef unsigned char uchar_t;
typedef unsigned int uint_t;

#define structof (type, member, pointer) ({ \
	const typeof (((type *) 0)->member) * __pointer = (pointer); \
	(type *) ((char *) __pointer - offsetof (type, member)); \
	})
