#include "arena.h"
#include "str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline bool is_power_of_two(usize x)
{
	return (x != 0) && ((x & (x - 1)) == 0);
}

static inline uintptr_t align_forward(uintptr_t ptr, usize align)
{
	return (ptr + (align - 1)) & ~(uintptr_t)(align - 1);
}

static ArenaBlock *arena_block_new(usize cap)
{
	ArenaBlock *b = malloc(sizeof(ArenaBlock) + cap);
	if (!b)
		return NULL;
	b->next = NULL;
	b->cap = cap;
	b->len = 0;
	return b;
}

Arena arena_new(usize default_block_cap)
{
	return (Arena){
		.first			   = NULL,
		.current		   = NULL,
		.default_block_cap = default_block_cap ? default_block_cap : ARENA_DEFAULT_BLOCK_CAP,
		.str_buf		   = NULL,
		.str_len		   = 0,
		.str_cap		   = 0,
		.in_str			   = false,
	};
}

void *arena_alloc_align(Arena *a, usize size, usize align)
{
	if (!a || size == 0)
		return NULL;
	if (align == 0 || !is_power_of_two(align)) {
		align = ARENA_DEFAULT_ALIGNMENT;
	}

	if (!a->current) {
		usize cap = a->default_block_cap ? a->default_block_cap : ARENA_DEFAULT_BLOCK_CAP;
		if (size + align > cap)
			cap = size + align;
		a->first = arena_block_new(cap);
		if (!a->first)
			return NULL;
		a->current = a->first;
	}

	uintptr_t curr = (uintptr_t)(a->current->data + a->current->len);
	uintptr_t aligned = align_forward(curr, align);
	usize padding = (usize)(aligned - curr);

	if (a->current->len + padding + size <= a->current->cap) {
		a->current->len += padding + size;
		return (void *)aligned;
	}

	usize next_cap = a->default_block_cap ? a->default_block_cap : ARENA_DEFAULT_BLOCK_CAP;
	if (size + align > next_cap)
		next_cap = size + align;

	ArenaBlock *next_block = arena_block_new(next_cap);
	if (!next_block)
		return NULL;

	a->current->next = next_block;
	a->current = next_block;

	curr = (uintptr_t)next_block->data;
	aligned = align_forward(curr, align);
	padding = (usize)(aligned - curr);

	next_block->len = padding + size;
	return (void *)aligned;
}

void *arena_alloc(Arena *a, usize size)
{
	return arena_alloc_align(a, size, ARENA_DEFAULT_ALIGNMENT);
}

static void arena_drop_string(Arena *a)
{
	/* A half-built string is transient; it dies with any rollback. */
	if (a->str_buf) {
		free(a->str_buf);
		a->str_buf = NULL;
	}
	a->str_len = 0;
	a->str_cap = 0;
	a->in_str = false;
}

void arena_reset(Arena *a)
{
	if (!a)
		return;
	arena_drop_string(a);
	for (ArenaBlock *b = a->first; b; b = b->next) {
		b->len = 0;
	}
	a->current = a->first;
}

void arena_destroy(Arena *a)
{
	if (!a)
		return;
	arena_drop_string(a);
	ArenaBlock *b = a->first;
	while (b) {
		ArenaBlock *next = b->next;
		free(b);
		b = next;
	}
	a->first = NULL;
	a->current = NULL;
}

char *rnstr_dup(Arena *a, const char *s)
{
	if (!s)
		return NULL;
	usize len = strlen(s);
	char *dup = (char *)arena_alloc_align(a, len + 1, 1);
	if (!dup)
		return NULL;
	memcpy(dup, s, len);
	dup[len] = '\0';
	return dup;
}

char *rnstr_sv_dup(Arena *a, StrView sv)
{
	if (!sv.data && sv.len == 0)
		return rnstr_dup(a, "");
	char *dup = (char *)arena_alloc_align(a, sv.len + 1, 1);
	if (!dup)
		return NULL;
	if (sv.data && sv.len > 0) {
		memcpy(dup, sv.data, sv.len);
	}
	dup[sv.len] = '\0';
	return dup;
}

PRINTF_FMT(2, 0) char *rnstr_vsprintf(Arena *a, const char *fmt, va_list ap)
{
	if (!a || !fmt)
		return NULL;

	va_list ap_copy;
	va_copy(ap_copy, ap);
	i32 needed = vsnprintf(NULL, 0, fmt, ap);
	if (needed < 0) {
		va_end(ap_copy);
		return NULL;
	}

	char *buf = arena_alloc_align(a, (usize)needed + 1, 1);
	if (!buf) {
		va_end(ap_copy);
		return NULL;
	}

	vsnprintf(buf, (usize)needed + 1, fmt, ap_copy);
	va_end(ap_copy);
	return buf;
}

PRINTF_FMT(2, 3) char *rnstr_sprintf(Arena *a, const char *fmt, ...)
{
	if (!a || !fmt)
		return NULL;

	va_list ap;
	va_start(ap, fmt);
	char *res = rnstr_vsprintf(a, fmt, ap);
	va_end(ap);
	return res;
}

char *rnstr_path(Arena *a, ...)
{
	if (!a)
		return NULL;

	va_list ap;
	va_start(ap, a);

	usize max_len = 0;
	const char *part;
	usize count = 0;
	while ((part = va_arg(ap, const char *)) != NULL) {
		if (part[0]) {
			max_len += strlen(part) + 1;
			count++;
		}
	}
	va_end(ap);

	if (count == 0)
		return rnstr_dup(a, "");

	char *dst = (char *)arena_alloc_align(a, max_len + 1, 1);
	if (!dst)
		return NULL;

	va_start(ap, a);
	char *p = dst;
	bool first = true;
	while ((part = va_arg(ap, const char *)) != NULL) {
		if (!part[0])
			continue;

		if (!first) {
			while (*part == '/')
				part++;
			if (!*part)
				continue;

			if (p > dst && *(p - 1) != '/') {
				*p++ = '/';
			}
		} else {
			first = false;
		}

		usize len = strlen(part);
		memcpy(p, part, len);
		p += len;
	}
	*p = '\0';
	va_end(ap);

	return dst;
}

/* ─── Incremental String Building (rnstr_*) ───
 *
 * The string under construction lives in a private heap buffer owned by the
 * arena. This keeps building fully independent of arena block switches:
 * interleaving arena_alloc / rnstr_sprintf while a string is open can never
 * corrupt it, and unreachable-on-growth logic can't miscompute offsets.
 */

/* Ensure room for `room` additional string bytes (+1 terminator). */
static bool rnstr_reserve(Arena *a, usize room)
{
	if (!a->in_str) {
		a->str_buf = NULL;
		a->str_len = 0;
		a->str_cap = 0;
		a->in_str = true;
	}

	usize needed = a->str_len + room + 1;
	if (needed <= a->str_cap)
		return true;

	usize ncap = a->str_cap ? a->str_cap : 256;
	while (ncap < needed)
		ncap *= 2;

	char *nb = realloc(a->str_buf, ncap);
	if (!nb)
		return false;
	a->str_buf = nb;
	a->str_cap = ncap;
	return true;
}

void strbuf_open(Arena *a)
{
	if (!a)
		return;
	/* Beginning while a string is already open continues it instead of
	 * clobbering the in-flight buffer. */
	if (a->in_str)
		return;
	a->str_buf = NULL;
	a->str_len = 0;
	a->str_cap = 0;
	a->in_str = true;
}

void strbuf_appendsv(Arena *a, StrView sv)
{
	if (!a || !sv.data || sv.len == 0)
		return;
	if (!rnstr_reserve(a, sv.len))
		return;
	memcpy(a->str_buf + a->str_len, sv.data, sv.len);
	a->str_len += sv.len;
}

void strbuf_append(Arena *a, const char *s)
{
	if (!a || !s || !*s)
		return;
	strbuf_appendsv(a, sv_fromc(s));
}

void strbuf_putc(Arena *a, char c)
{
	char buf[1] = {c};
	strbuf_appendsv(a, sv_new(buf, 1));
}

void strbuf_printf(Arena *a, const char *fmt, ...)
{
	if (!a || !fmt)
		return;

	va_list ap, ap_copy;
	va_start(ap, fmt);
	i32 needed = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (needed < 0)
		return;

	if (!rnstr_reserve(a, (usize)needed))
		return;

	va_start(ap_copy, fmt);
	vsnprintf(a->str_buf + a->str_len, (usize)needed + 1, fmt, ap_copy);
	va_end(ap_copy);

	a->str_len += (usize)needed;
}

const char *strbuf_finish(Arena *a)
{
	if (!a || !a->in_str)
		return "";

	char *out = arena_alloc_align(a, a->str_len + 1, 1);
	if (out) {
		if (a->str_len > 0) {
			memcpy(out, a->str_buf, a->str_len);
		}
		out[a->str_len] = '\0';
	}

	free(a->str_buf);
	a->str_buf = NULL;
	a->str_len = 0;
	a->str_cap = 0;
	a->in_str = false;

	return out ? out : "";
}
