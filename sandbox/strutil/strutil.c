#include "strutil.h"

#include <ctype.h>

void str_upper(char *dst, unsigned long cap, const char *src)
{
	unsigned long i = 0;
	for (; src[i] && i < cap - 1; ++i) {
		dst[i] = (char)tolower((unsigned char)src[i]);
	}
	dst[i] = '\0';
}
