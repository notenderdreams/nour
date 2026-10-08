#include "arena.h"
#include "str.h"
#include "test.h"
#include <stdint.h>
#include <string.h>

TEST(Arena, basic_and_alignment)
{
	Arena a = arena_new(1024);
	defer(arena_destroy, &a);

	// Default alignment
	void *p1 = arena_alloc(&a, 17);
	ASSERT_NOT_NULL(p1);
	ASSERT_EQ((uintptr_t)p1 % ARENA_DEFAULT_ALIGNMENT, 0);

	// Explicit power-of-two alignments: 1, 2, 4, 8, 16, 32, 64, 128
	usize alignments[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
	for (usize i = 0; i < sizeof(alignments) / sizeof(alignments[0]); ++i) {
		usize align = alignments[i];
		void *p = arena_alloc_align(&a, 3, align);
		ASSERT_NOT_NULL(p);
		ASSERT_EQ((uintptr_t)p % align, 0);
	}
}

TEST(Arena, string_helpers)
{
	Arena a = arena_new(512);
	defer(arena_destroy, &a);

	// rnstr_dup
	char *s1 = rnstr_dup(&a, "hello world");
	ASSERT_NOT_NULL(s1);
	ASSERT_STR_EQ(s1, "hello world");

	// rnstr_sv_dup
	char *s2 = rnstr_sv_dup(&a, SV("nour-lang"));
	ASSERT_NOT_NULL(s2);
	ASSERT_STR_EQ(s2, "nour-lang");

	// rnstr_sprintf
	char *fmt = rnstr_sprintf(&a, "key: %s, value: %d", "port", 8080);
	ASSERT_NOT_NULL(fmt);
	ASSERT_STR_EQ(fmt, "key: port, value: 8080");

	// rnstr_path (path joiner collapsing excess slashes)
	char *p1 = rnstr_path(&a, "root", "src", "main.c", NULL);
	ASSERT_STR_EQ(p1, "root/src/main.c");

	char *p2 = rnstr_path(&a, "/var/log/", "/nour/", "build.log", NULL);
	ASSERT_STR_EQ(p2, "/var/log/nour/build.log");

	char *p3 = rnstr_path(&a, "base", "", "sub", NULL);
	ASSERT_STR_EQ(p3, "base/sub");
}

TEST(Arena, chunk_growth)
{
	// Tiny 128-byte block capacity to force chunk allocations
	Arena a = arena_new(128);

	// Allocate 10 items of 100 bytes each -> requires multiple chained chunks
	char *items[10];
	for (usize i = 0; i < 10; ++i) {
		items[i] = rnstr_sprintf(&a, "chunk_item_%zu_data_payload_string", i);
		ASSERT_NOT_NULL(items[i]);
	}

	// Verify all items are intact
	for (usize i = 0; i < 10; ++i) {
		char expected[64];
		snprintf(expected, sizeof(expected), "chunk_item_%zu_data_payload_string", i);
		ASSERT_STR_EQ(items[i], expected);
	}

	arena_destroy(&a);
}

TEST(Arena, rnstr_building)
{
	Arena a = arena_new(32); // Small capacity to force block growth

	rnstr_begin(&a);
	rnstr_append(&a, "cc -o ");
	rnstr_appendsv(&a, SV("app"));
	rnstr_putc(&a, ' ');
	rnstr_printf(&a, "-I%s -D%s=%d", "include", "DEBUG", 1);
	for (int i = 0; i < 3; ++i) {
		rnstr_printf(&a, " src%d.c", i);
	}
	const char *cmd = rnstr_finish(&a);

	ASSERT_STR_EQ(cmd, "cc -o app -Iinclude -DDEBUG=1 src0.c src1.c src2.c");

	// Test subsequent rnstr build
	rnstr_begin(&a);
	rnstr_printf(&a, "next: %d", 999);
	const char *next_cmd = rnstr_finish(&a);
	ASSERT_STR_EQ(next_cmd, "next: 999");

	arena_destroy(&a);
}

static bool helper_scratch_func(void)
{
	arena_scratch(s, 512);
	char *str = rnstr_sprintf(&s, "scratch_%d", 42);
	return (str != NULL && strcmp(str, "scratch_42") == 0);
}

TEST(Arena, scratch_and_cleanup)
{
	// Test arena_destroy and arena_reset
	Arena a = arena_new(1024);
	void *p = arena_alloc(&a, 128);
	ASSERT_NOT_NULL(p);
	arena_reset(&a);
	void *p2 = arena_alloc(&a, 64);
	ASSERT_NOT_NULL(p2);
	arena_destroy(&a);

	// Test arena_scratch with default cap
	{
		arena_scratch(local);
		char *msg = rnstr_dup(&local, "ephemeral");
		ASSERT_STR_EQ(msg, "ephemeral");
	}

	// Test arena_scratch with custom cap in helper function (destroyed on return)
	ASSERT_TRUE(helper_scratch_func());
}

static int g_defer_trace[4];
static int g_defer_idx = 0;

static void trace_step(void *val)
{
	g_defer_trace[g_defer_idx++] = (int)(intptr_t)val;
}

static void trace_zero(void)
{
	g_defer_trace[g_defer_idx++] = 99;
}

static void helper_defer_lifo(void)
{
	defer(trace_step, (void *)1);
	defer(trace_step, (void *)2);
	defer(trace_zero);
	defer(trace_step, (void *)3);
}

TEST(Arena, defer_lifo_execution)
{
	g_defer_idx = 0;
	helper_defer_lifo();

	// Must execute in LIFO order: 3, 99, 2, 1
	ASSERT_EQ(g_defer_idx, 4);
	ASSERT_EQ(g_defer_trace[0], 3);
	ASSERT_EQ(g_defer_trace[1], 99);
	ASSERT_EQ(g_defer_trace[2], 2);
	ASSERT_EQ(g_defer_trace[3], 1);
}
