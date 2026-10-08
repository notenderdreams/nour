#pragma once

#include "str.h"
#include "types.h"
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#define ARENA_DEFAULT_ALIGNMENT _Alignof(max_align_t)
#define ARENA_DEFAULT_BLOCK_CAP (64 * 1024)

typedef struct ArenaBlock ArenaBlock;

struct ArenaBlock {
	ArenaBlock *next;
	usize		cap;
	usize		len;
	alignas(max_align_t) u8 data[];
};

typedef struct Arena {
	ArenaBlock *first;
	ArenaBlock *current;
	usize		default_block_cap;
	usize		str_start;
	bool		in_str;
} Arena;

// Lifecycle
Arena arena_new(usize default_block_cap);
void  arena_reset(Arena *a);
void  arena_destroy(Arena *a);

// Raw Allocations
void *arena_alloc_align(Arena *a, usize size, usize align);
void *arena_alloc(Arena *a, usize size);

// String Helpers (rnstr_*)
char *rnstr_dup(Arena *a, const char *s);
char *rnstr_sv_dup(Arena *a, StrView sv);
PRINTF_FMT(2, 3) char *rnstr_sprintf(Arena *a, const char *fmt, ...);
PRINTF_FMT(2, 0) char *rnstr_vsprintf(Arena *a, const char *fmt, va_list ap);
char *rnstr_path(Arena *a, ...) __attribute__((sentinel));

// Incremental String Building (rnstr_*)
void rnstr_begin(Arena *a);
void rnstr_append(Arena *a, const char *s);
void rnstr_appendsv(Arena *a, StrView sv);
void rnstr_putc(Arena *a, char c);
PRINTF_FMT(2, 3) void rnstr_printf(Arena *a, const char *fmt, ...);
const char *rnstr_finish(Arena *a);

// Scoped scratch arena: arena_scratch(name) or arena_scratch(name, cap)
static inline void arena_cleanup(Arena *a)
{
	arena_destroy(a);
}

#define _ARENA_SCRATCH_1(name) Arena name __attribute__((cleanup(arena_cleanup))) = arena_new(0)
#define _ARENA_SCRATCH_2(name, cap) \
	Arena name __attribute__((cleanup(arena_cleanup))) = arena_new(cap)
#define _ARENA_SCRATCH_DISPATCH(_1, _2, NAME, ...) NAME

#define arena_scratch(...) \
	_ARENA_SCRATCH_DISPATCH(__VA_ARGS__, _ARENA_SCRATCH_2, _ARENA_SCRATCH_1, ~)(__VA_ARGS__)
