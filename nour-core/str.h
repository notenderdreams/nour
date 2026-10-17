#pragma once

#include "types.h"
#include <stdbool.h>
#include <string.h>

/* ─── StrView (Non-owning read-only slice) ─── */

typedef struct StrView {
	const char *data;
	usize		len;
} StrView;

#define SV(lit) ((StrView){.data = "" lit, .len = sizeof(lit) - 1})

static inline StrView sv_new(const char *data, usize len)
{
	return (StrView){.data = data, .len = len};
}

static inline StrView sv_fromc(const char *s)
{
	if (!s)
		return (StrView){.data = NULL, .len = 0};
	return (StrView){.data = s, .len = strlen(s)};
}

static inline bool sv_empty(StrView sv)
{
	return sv.len == 0;
}

bool sv_eq(StrView a, StrView b);
bool sv_eq_cstr(StrView sv, const char *cstr);

bool sv_starts(StrView sv, StrView prefix);
bool sv_ends(StrView sv, StrView suffix);
bool sv_contains(StrView sv, StrView needle);
bool sv_contains_cstr(StrView sv, const char *cstr);
i32	 sv_cmp(StrView a, StrView b);

StrView sv_trim(StrView sv);
StrView sv_sub(StrView sv, usize start, usize len);
bool	sv_split(StrView sv, char delimiter, StrView *left, StrView *right);

char *sv_dup(StrView sv);
