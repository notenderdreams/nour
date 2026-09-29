#pragma once

#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "types.h"

#define TF_WIDTH     44
#define MAX_CASES    128

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_GRAY    "\033[90m"

typedef struct {
	char   name[64];
	double duration_ms;
	bool   passed;
	char   error_msg[256];
	char   error_file[128];
	int    error_line;
} TestCaseResult;

typedef struct {
	const char    *name;
	TestCaseResult cases[MAX_CASES];
	int            count;
	int            passed_count;
	uint64_t       start_ns;
} SuiteContext;

typedef struct {
	int      total;
	int      passed;
	int      failed;
	uint64_t start_ns;
} TestContext;

typedef void (*TestFn)(void);

typedef struct TestEntry {
	const char       *suite;
	const char       *name;
	TestFn            fn;
	struct TestEntry *next;
} TestEntry;

#ifdef TEST_MAIN
TestContext  g_test_ctx = { 0 };
SuiteContext g_suite = { 0 };
bool         g_cur_test_failed = false;
char         g_cur_test_error[256] = { 0 };
char         g_cur_test_file[128] = { 0 };
int          g_cur_test_line = 0;
bool         g_color = true;
TestEntry   *g_test_registry = NULL;
#else
extern TestContext  g_test_ctx;
extern SuiteContext g_suite;
extern bool         g_cur_test_failed;
extern char         g_cur_test_error[256];
extern char         g_cur_test_file[128];
extern int          g_cur_test_line;
extern bool         g_color;
extern TestEntry   *g_test_registry;
#endif

static inline void tf_init_color(void)
{
	g_color = isatty(fileno(stdout)) && !getenv("NO_COLOR");
}

static inline void tf_printf(const char *fmt, ...)
{
	char buf[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (g_color) {
		fputs(buf, stdout);
		return;
	}

	for (const char *p = buf; *p; ++p) {
		if (p[0] == '\033' && p[1] == '[') {
			while (*p && *p != 'm') ++p;
			if (!*p) break;
			continue;
		}
		putchar(*p);
	}
}

static inline uint64_t tf_time_ns(void)
{
	struct timespec ts;
#if defined(CLOCK_MONOTONIC)
	clock_gettime(CLOCK_MONOTONIC, &ts);
#else
	clock_gettime(CLOCK_REALTIME, &ts);
#endif
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void tf_format_time(double ms, char *buf, size_t buf_size)
{
	char raw[32];
	if (ms >= 10.0) {
		snprintf(raw, sizeof(raw), "%.1f ms", ms);
	} else {
		snprintf(raw, sizeof(raw), "%.2f ms", ms);
	}
	snprintf(buf, buf_size, "%10s", raw);
}

#define SUITE_BEGIN(suite_name) \
	do { \
		g_suite.name = (suite_name); \
		g_suite.count = 0; \
		g_suite.passed_count = 0; \
		g_suite.start_ns = tf_time_ns(); \
	} while (0)

static inline void record_test_result(const char *name, double ms, bool passed,
                                      const char *err_msg, const char *err_file, int err_line)
{
	if (g_suite.count >= MAX_CASES) {
		fprintf(stderr, "suite '%s' exceeded %d cases\n", g_suite.name ? g_suite.name : "<unknown>", MAX_CASES);
		abort();
	}
	TestCaseResult *tc = &g_suite.cases[g_suite.count++];
	snprintf(tc->name, sizeof(tc->name), "%s", name);
	tc->duration_ms = ms;
	tc->passed = passed;
	if (passed) {
		g_suite.passed_count++;
		g_test_ctx.passed++;
	} else {
		g_test_ctx.failed++;
		snprintf(tc->error_msg, sizeof(tc->error_msg), "%s", err_msg ? err_msg : "unknown failure");
		snprintf(tc->error_file, sizeof(tc->error_file), "%s", err_file ? err_file : "");
		tc->error_line = err_line;
	}
	g_test_ctx.total++;
}

static inline void suite_end(void)
{
	if (g_suite.count == 0) return;
	double suite_ms = (double)(tf_time_ns() - g_suite.start_ns) / 1000000.0;
	bool all_passed = (g_suite.passed_count == g_suite.count);

	const char *icon = all_passed ? (ANSI_BOLD ANSI_GREEN "✓" ANSI_RESET) : (ANSI_BOLD ANSI_RED "✗" ANSI_RESET);
	char title[64];
	snprintf(title, sizeof(title), "%s %d/%d", g_suite.name, g_suite.passed_count, g_suite.count);

	int w_left = 2 + (int)strlen(title);
	int pad = TF_WIDTH - w_left;
	if (pad < 1) pad = 1;

	char time_str[32];
	tf_format_time(suite_ms, time_str, sizeof(time_str));

	tf_printf("%s " ANSI_BOLD "%s" ANSI_RESET, icon, title);
	for (int i = 0; i < pad; ++i) tf_printf(" ");
	tf_printf(ANSI_GRAY "%s" ANSI_RESET "\n", time_str);

	for (int i = 0; i < g_suite.count; ++i) {
		TestCaseResult *tc = &g_suite.cases[i];
		bool is_last = (i == g_suite.count - 1);
		const char *branch = is_last ? "└─" : "├─";

		char display_name[64];
		int max_name_len = TF_WIDTH - 6;
		if ((int)strlen(tc->name) > max_name_len) {
			int keep_len = max_name_len - 3;
			if (keep_len < 0) keep_len = 0;
			snprintf(display_name, sizeof(display_name), "%.*s...", keep_len, tc->name);
		} else {
			snprintf(display_name, sizeof(display_name), "%s", tc->name);
		}

		int w_test = 6 + (int)strlen(display_name);
		int dots_count = TF_WIDTH - w_test;
		if (dots_count < 0) dots_count = 0;

		char tc_time[32];
		tf_format_time(tc->duration_ms, tc_time, sizeof(tc_time));

		tf_printf("  " ANSI_GRAY "%s" ANSI_RESET " %s ", branch, display_name);
		tf_printf(ANSI_GRAY);
		for (int d = 0; d < dots_count; ++d) {
			tf_printf("·");
		}
		tf_printf(ANSI_RESET);
		tf_printf(ANSI_GRAY "%s" ANSI_RESET "\n", tc_time);

		if (!tc->passed) {
			tf_printf("    " ANSI_RED "✘ Assertion failed:" ANSI_RESET " %s\n", tc->error_msg);
			if (tc->error_file[0]) {
				tf_printf("      " ANSI_GRAY "at %s:%d" ANSI_RESET "\n", tc->error_file, tc->error_line);
			}
		}
	}
	tf_printf("\n");
}

#define SUITE_END() suite_end()

// Automatic registration via constructor
static inline void tf_register(const char *suite, const char *name, TestFn fn)
{
	TestEntry *entry = (TestEntry *)malloc(sizeof(TestEntry));
	if (!entry) abort();
	entry->suite = suite;
	entry->name  = name;
	entry->fn    = fn;
	entry->next  = NULL;

	if (!g_test_registry) {
		g_test_registry = entry;
	} else {
		TestEntry *curr = g_test_registry;
		while (curr->next) curr = curr->next;
		curr->next = entry;
	}
}

#define TEST(suite, name) \
	static void t_##suite##_##name(void); \
	__attribute__((constructor)) static void reg_##suite##_##name(void) { \
		tf_register(#suite, #name, t_##suite##_##name); \
	} \
	static void t_##suite##_##name(void)

#define TEST_FAIL_AT(file, line, fmt, ...) \
	do { \
		g_cur_test_failed = true; \
		snprintf(g_cur_test_error, sizeof(g_cur_test_error), fmt, ##__VA_ARGS__); \
		snprintf(g_cur_test_file, sizeof(g_cur_test_file), "%s", file); \
		g_cur_test_line = line; \
		return; \
	} while (0)

#define ASSERT_TRUE(expr) \
	do { \
		if (!(expr)) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected '%s' to be true", #expr); \
		} \
	} while (0)

#define ASSERT_FALSE(expr) \
	do { \
		if (expr) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected '%s' to be false", #expr); \
		} \
	} while (0)

#define ASSERT_EQ(actual, expected) \
	do { \
		int64_t _act = (int64_t)(actual); \
		int64_t _exp = (int64_t)(expected); \
		if (_act != _exp) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected %s == %s (got %" PRId64 ", expected %" PRId64 ")", \
			             #actual, #expected, _act, _exp); \
		} \
	} while (0)

#define ASSERT_NE(actual, expected) \
	do { \
		int64_t _act = (int64_t)(actual); \
		int64_t _exp = (int64_t)(expected); \
		if (_act == _exp) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected %s != %s (both are %" PRId64 ")", \
			             #actual, #expected, _act); \
		} \
	} while (0)

#define ASSERT_PTR_EQ(actual, expected) \
	do { \
		const void *_act = (const void *)(actual); \
		const void *_exp = (const void *)(expected); \
		if (_act != _exp) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected %s == %s (got %p, expected %p)", \
			             #actual, #expected, _act, _exp); \
		} \
	} while (0)

#define ASSERT_NOT_NULL(actual) \
	do { \
		const void *_act = (const void *)(actual); \
		if (_act == NULL) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected '%s' to not be NULL", #actual); \
		} \
	} while (0)

#define ASSERT_NULL(actual) \
	do { \
		const void *_act = (const void *)(actual); \
		if (_act != NULL) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected '%s' to be NULL (got %p)", #actual, _act); \
		} \
	} while (0)

#define ASSERT_STR_EQ(actual, expected) \
	do { \
		const char *_act = (const char *)(actual); \
		const char *_exp = (const char *)(expected); \
		if (!_act && !_exp) break; \
		if (!_act || !_exp || strcmp(_act, _exp) != 0) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "\n      Got:      \"%s\"\n      Expected: \"%s\"", \
			             _act ? _act : "(null)", _exp ? _exp : "(null)"); \
		} \
	} while (0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
	do { \
		const char *_hay = (const char *)(haystack); \
		const char *_nee = (const char *)(needle); \
		if (!_hay || !_nee || strstr(_hay, _nee) == NULL) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected string to contain \"%s\"\n      In: \"%s\"", \
			             _nee ? _nee : "(null)", _hay ? _hay : "(null)"); \
		} \
	} while (0)

#define ASSERT_RESULT_OK(res_expr) \
	do { \
		Result _r __attribute__((cleanup(result_free))) = (res_expr); \
		if (!_r.ok) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Result is Err: %s", _r.error ? _r.error : "(unknown)"); \
		} \
	} while (0)

#define ASSERT_RESULT_ERR(res_expr) \
	do { \
		Result _r __attribute__((cleanup(result_free))) = (res_expr); \
		if (_r.ok) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected Result to be Err, but got Ok"); \
		} \
	} while (0)

#define ASSERT_RESULT_ERR_CONTAINS(res_expr, needle) \
	do { \
		Result _r __attribute__((cleanup(result_free))) = (res_expr); \
		if (_r.ok) { \
			TEST_FAIL_AT(__FILE__, __LINE__, "Expected Result to be Err, but got Ok"); \
		} else { \
			ASSERT_STR_CONTAINS(_r.error, (needle)); \
		} \
	} while (0)

static inline void tf_free_str(char **p)
{
	if (*p) {
		free(*p);
		*p = NULL;
	}
}

#define ASSERT_PREPROCESS(input_code, ...) \
	do { \
		char *_out __attribute__((cleanup(tf_free_str))) = NULL; \
		ASSERT_RESULT_OK(preprocess_str((input_code), &_out)); \
		ASSERT_NOT_NULL(_out); \
		const char *_needles[] = { __VA_ARGS__, NULL }; \
		for (int _i = 0; _needles[_i]; ++_i) { \
			ASSERT_STR_CONTAINS(_out, _needles[_i]); \
		} \
	} while (0)

#define ASSERT_PREPROCESS_ERR(input_code) \
	do { \
		char *_out __attribute__((cleanup(tf_free_str))) = NULL; \
		ASSERT_RESULT_ERR(preprocess_str((input_code), &_out)); \
		ASSERT_NULL(_out); \
	} while (0)

static inline bool test_tmpdir_make(char *buf, size_t cap)
{
	const char *tmp = getenv("TMPDIR");
	if (!tmp || !*tmp) tmp = "/tmp";
	snprintf(buf, cap, "%s/nour_test_XXXXXX", tmp);
	if (!mkdtemp(buf)) {
		buf[0] = '\0';
		return false;
	}
	return true;
}

static inline void test_tmpdir_cleanup(const char *path)
{
	if (!path || !path[0]) return;
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
	system(cmd);
}

typedef struct {
	char path[256];
} TmpDir;

static inline void tmpdir_drop(TmpDir *t)
{
	if (t->path[0]) {
		test_tmpdir_cleanup(t->path);
		t->path[0] = '\0';
	}
}

#define TMPDIR_AUTO(n) \
	TmpDir n __attribute__((cleanup(tmpdir_drop))) = { { 0 } }; \
	test_tmpdir_make(n.path, sizeof(n.path))

void loader_close(void);

static inline void auto_loader_close(int *dummy)
{
	(void)dummy;
	loader_close();
}

#define LOADER_AUTO int _auto_ld __attribute__((cleanup(auto_loader_close))) = 0; (void)_auto_ld

#define TEST_SILENT(...) \
	do { \
		int _saved = dup(fileno(stderr)); \
		int _null  = open("/dev/null", O_WRONLY); \
		if (_null >= 0) { \
			dup2(_null, fileno(stderr)); \
			close(_null); \
		} \
		__VA_ARGS__; \
		if (_saved >= 0) { \
			dup2(_saved, fileno(stderr)); \
			close(_saved); \
		} \
	} while (0)

static inline int test_summary(void)
{
	double total_ms = (double)(tf_time_ns() - g_test_ctx.start_ns) / 1000000.0;
	tf_printf(ANSI_GRAY "──────────────────────────────────────────────────────" ANSI_RESET "\n");

	if (g_test_ctx.failed == 0) {
		tf_printf(ANSI_BOLD ANSI_GREEN "✓" ANSI_RESET " " ANSI_BOLD "%d passed" ANSI_RESET
		          "  ·  " ANSI_GRAY "%d failed" ANSI_RESET
		          "  ·  " ANSI_GRAY "%.0f ms" ANSI_RESET "\n\n",
		          g_test_ctx.passed, g_test_ctx.failed, total_ms);
		return 0;
	} else {
		tf_printf(ANSI_BOLD ANSI_RED "✗" ANSI_RESET " " ANSI_BOLD "%d passed" ANSI_RESET
		          "  ·  " ANSI_BOLD ANSI_RED "%d failed" ANSI_RESET
		          "  ·  " ANSI_GRAY "%.0f ms" ANSI_RESET "\n\n",
		          g_test_ctx.passed, g_test_ctx.failed, total_ms);
		return 1;
	}
}

static inline void tf_run_suite_tests(const char *suite_name, const char *filter)
{
	bool       suite_started = false;
	TestEntry *entry         = g_test_registry;

	while (entry) {
		if (strcmp(entry->suite, suite_name) != 0) {
			entry = entry->next;
			continue;
		}

		if (filter && !strstr(entry->suite, filter) && !strstr(entry->name, filter)) {
			entry = entry->next;
			continue;
		}

		if (!suite_started) {
			SUITE_BEGIN(suite_name);
			suite_started = true;
		}

		g_cur_test_failed   = false;
		g_cur_test_error[0] = '\0';
		g_cur_test_file[0]  = '\0';
		g_cur_test_line     = 0;

		uint64_t start_ns = tf_time_ns();
		entry->fn();
		double ms = (double)(tf_time_ns() - start_ns) / 1000000.0;

		record_test_result(entry->name, ms, !g_cur_test_failed, g_cur_test_error, g_cur_test_file, g_cur_test_line);

		entry = entry->next;
	}

	if (suite_started) {
		SUITE_END();
	}
}

static inline void tf_run_registered(const char *filter)
{
	static const char *ordered_suites[] = { "Preprocessor", "Loader", "Builder", NULL };
	const char *ran_suites[64] = { 0 };
	int num_ran = 0;

	for (int i = 0; ordered_suites[i]; ++i) {
		tf_run_suite_tests(ordered_suites[i], filter);
		if (num_ran < 64) {
			ran_suites[num_ran++] = ordered_suites[i];
		}
	}

	TestEntry *entry = g_test_registry;
	while (entry) {
		bool already_ran = false;
		for (int i = 0; i < num_ran; ++i) {
			if (strcmp(entry->suite, ran_suites[i]) == 0) {
				already_ran = true;
				break;
			}
		}
		if (!already_ran) {
			tf_run_suite_tests(entry->suite, filter);
			if (num_ran < 64) {
				ran_suites[num_ran++] = entry->suite;
			}
		}
		entry = entry->next;
	}
}

