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
		.str_start		   = 0,
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

void arena_reset(Arena *a)
{
	if (!a)
		return;
	for (ArenaBlock *b = a->first; b; b = b->next) {
		b->len = 0;
	}
	a->current = a->first;
	a->str_start = 0;
	a->in_str = false;
}

void arena_destroy(Arena *a)
{
	if (!a)
		return;
	ArenaBlock *b = a->first;
	while (b) {
		ArenaBlock *next = b->next;
		free(b);
		b = next;
	}
	a->first = NULL;
	a->current = NULL;
	a->str_start = 0;
	a->in_str = false;
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

void rnstr_begin(Arena *a)
{
	if (!a)
		return;
	if (!a->current) {
		usize cap = a->default_block_cap ? a->default_block_cap : ARENA_DEFAULT_BLOCK_CAP;
		a->first = arena_block_new(cap);
		a->current = a->first;
	}
	if (!a->current)
		return;
	a->str_start = a->current->len;
	a->in_str = true;
}

void rnstr_appendsv(Arena *a, StrView sv)
{
	if (!a || !sv.data || sv.len == 0)
		return;
	if (!a->in_str)
		rnstr_begin(a);
	if (!a->current)
		return;

	usize avail = (a->current->cap > a->current->len) ? (a->current->cap - a->current->len) : 0;

	if (avail >= sv.len + 1) {
		memcpy(a->current->data + a->current->len, sv.data, sv.len);
		a->current->len += sv.len;
		return;
	}

	usize cur_len = a->current->len - a->str_start;
	usize needed = cur_len + sv.len + 1;
	usize next_cap = a->default_block_cap > needed ? a->default_block_cap : needed * 2;
	ArenaBlock *nb = arena_block_new(next_cap);
	if (!nb)
		return;

	if (cur_len > 0) {
		memcpy(nb->data, a->current->data + a->str_start, cur_len);
	}
	memcpy(nb->data + cur_len, sv.data, sv.len);
	nb->len = cur_len + sv.len;

	a->current->next = nb;
	a->current = nb;
	a->str_start = 0;
}

void rnstr_append(Arena *a, const char *s)
{
	if (!a || !s || !*s)
		return;
	rnstr_appendsv(a, sv_fromc(s));
}

void rnstr_putc(Arena *a, char c)
{
	char buf[1] = { c };
	rnstr_appendsv(a, sv_new(buf, 1));
}

void rnstr_printf(Arena *a, const char *fmt, ...)
{
	if (!a || !fmt)
		return;
	if (!a->in_str)
		rnstr_begin(a);
	if (!a->current)
		return;

	va_list ap, ap_copy;
	va_start(ap, fmt);
	va_copy(ap_copy, ap);

	usize avail = (a->current->cap > a->current->len) ? (a->current->cap - a->current->len) : 0;
	int needed =
		vsnprintf(avail > 0 ? (char *)(a->current->data + a->current->len) : NULL, avail, fmt, ap);
	va_end(ap);

	if (needed < 0) {
		va_end(ap_copy);
		return;
	}

	if ((usize)needed < avail) {
		a->current->len += (usize)needed;
		va_end(ap_copy);
		return;
	}

	usize cur_len = a->current->len - a->str_start;
	usize total_needed = cur_len + (usize)needed + 1;
	usize next_cap = a->default_block_cap > total_needed ? a->default_block_cap : total_needed * 2;
	ArenaBlock *nb = arena_block_new(next_cap);
	if (!nb) {
		va_end(ap_copy);
		return;
	}

	if (cur_len > 0) {
		memcpy(nb->data, a->current->data + a->str_start, cur_len);
	}

	vsnprintf((char *)(nb->data + cur_len), (usize)needed + 1, fmt, ap_copy);
	va_end(ap_copy);

	nb->len = cur_len + (usize)needed;
	a->current->next = nb;
	a->current = nb;
	a->str_start = 0;
}

const char *rnstr_finish(Arena *a)
{
	if (!a || !a->in_str || !a->current)
		return "";

	if (a->current->len < a->current->cap) {
		a->current->data[a->current->len] = '\0';
		a->current->len += 1;
	} else {
		rnstr_putc(a, '\0');
	}

	const char *result = (const char *)(a->current->data + a->str_start);
	a->in_str = false;
	a->str_start = 0;
	a->current->len = align_forward(a->current->len, ARENA_DEFAULT_ALIGNMENT);
	return result;
}
