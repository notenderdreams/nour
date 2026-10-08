#include "str.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── StrView Implementation ─── */

bool sv_eq(StrView a, StrView b)
{
	if (a.len != b.len)
		return false;
	if (a.len == 0)
		return true;

	return memcmp(a.data, b.data, a.len) == 0;
}

bool sv_eqc(StrView sv, const char *cstr)
{
	if (!cstr)
		return sv.len == 0;

	usize cstr_len = strlen(cstr);
	if (sv.len != cstr_len)
		return false;

	return memcmp(sv.data, cstr, sv.len) == 0;
}

bool sv_starts(StrView sv, StrView prefix)
{
	if (sv.len < prefix.len)
		return false;
	return memcmp(sv.data, prefix.data, prefix.len) == 0;
}

bool sv_ends(StrView sv, StrView suffix)
{
	if (sv.len < suffix.len)
		return false;
	return memcmp(sv.data + (sv.len - suffix.len), suffix.data, suffix.len) == 0;
}

bool sv_contains(StrView sv, StrView needle)
{
	if (needle.len == 0)
		return true;
	if (sv.len < needle.len || !sv.data || !needle.data)
		return false;
	for (usize i = 0; i <= sv.len - needle.len; ++i) {
		if (memcmp(sv.data + i, needle.data, needle.len) == 0)
			return true;
	}
	return false;
}

bool sv_containsc(StrView sv, const char *cstr)
{
	return sv_contains(sv, sv_fromc(cstr));
}

i32 sv_cmp(StrView a, StrView b)
{
	usize min_len = a.len < b.len ? a.len : b.len;
	if (min_len) {
		i32 cmp = memcmp(a.data, b.data, min_len);
		if (cmp)
			return cmp < 0 ? -1 : 1;
	}
	if (a.len < b.len)
		return -1;
	if (a.len > b.len)
		return 1;

	return 0;
}

StrView sv_trim(StrView sv)
{
	const char *start = sv.data;
	const char *end = sv.data + sv.len;

	while (start < end && isspace((unsigned char)*start))
		++start;
	while (end > start && isspace((unsigned char)*(end - 1)))
		--end;

	return (StrView){
		.data = start,
		.len  = (usize)(end - start),
	};
}

StrView sv_sub(StrView sv, usize start, usize len)
{
	if (start >= sv.len) {
		return (StrView){ .data = sv.data + sv.len, .len = 0 };
	}
	if (len > sv.len - start) {
		len = sv.len - start;
	}
	return (StrView){ .data = sv.data + start, .len = len };
}

bool sv_split(StrView sv, char delimiter, StrView *left, StrView *right)
{
	for (usize i = 0; i < sv.len; ++i) {
		if (sv.data[i] == delimiter) {
			if (left) {
				left->data = sv.data;
				left->len = i;
			}
			if (right) {
				right->data = sv.data + i + 1;
				right->len = sv.len - i - 1;
			}
			return true;
		}
	}
	if (left)
		*left = sv;
	if (right)
		*right = (StrView){ .data = sv.data + sv.len, .len = 0 };
	return false;
}

char *sv_dup(StrView sv)
{
	char *buf = malloc(sv.len + 1);
	if (!buf)
		return NULL;
	if (sv.len > 0 && sv.data) {
		memcpy(buf, sv.data, sv.len);
	}
	buf[sv.len] = '\0';
	return buf;
}
