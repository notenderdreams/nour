#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

typedef int8_t	 i8;
typedef uint8_t	 u8;
typedef int32_t	 i32;
typedef uint32_t u32;
typedef int64_t	 i64;
typedef uint64_t u64;
typedef float	 f32;
typedef double	 f64;
typedef size_t	 usize;

typedef struct {
	bool		ok;
	void	   *value;
	const char *error;
} Result;

#define PRINTF_FMT(fmt, args) __attribute__((format(printf, fmt, args)))

static inline Result Ok(void *value)
{
	return (Result){
		.ok	   = true,
		.value = value,
		.error = NULL,
	};
}

static inline Result _vresult_err(const char *fmt, va_list args)
{
	va_list args_copy;
	va_copy(args_copy, args);

	i32 len = vsnprintf(NULL, 0, fmt, args);

	char *buf = NULL;
	if (len >= 0) {
		buf = malloc((usize)len + 1);
		if (buf) {
			vsnprintf(buf, (usize)len + 1, fmt, args_copy);
		}
	}
	va_end(args_copy);

	return (Result){
		.ok	   = false,
		.value = NULL,
		.error = buf,
	};
}

PRINTF_FMT(1, 2) static inline Result _err_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Result res = _vresult_err(fmt, args);
	va_end(args);
	return res;
}

#include "error.h"

static inline void result_free(Result *res)
{
	if (res && !res->ok && res->error) {
		free((void *)res->error);
		res->error = NULL;
	}
}

#define Try(expr)           \
	do {                    \
		Result _r = (expr); \
		if (!_r.ok)         \
			return _r;      \
	} while (0)

#define Unwrap(expr)        \
	__extension__({         \
		Result _r = (expr); \
		if (!_r.ok)         \
			return _r;      \
		_r.value;           \
	})

// Scope Cleanup (defer)
typedef void (*defer_fn1_t)(void *);
typedef void (*defer_fn0_t)(void);

typedef struct {
	void *fn;
	void *arg;
	int	  has_arg;
} defer_entry_t;

static inline void _defer_run(defer_entry_t *entry)
{
	if (!entry || !entry->fn)
		return;
	if (entry->has_arg) {
		((defer_fn1_t)entry->fn)(entry->arg);
	} else {
		((defer_fn0_t)entry->fn)();
	}
}

#define _DEFER_CONCAT2(a, b) a##b
#define _DEFER_CONCAT(a, b)	 _DEFER_CONCAT2(a, b)

#define _DEFER_1(_fn)                                                                       \
	__attribute__((cleanup(_defer_run))) defer_entry_t _DEFER_CONCAT(_defer_, __LINE__) = { \
		.fn = (void *)(_fn), .arg = NULL, .has_arg = 0                                      \
	}

#define _DEFER_2(_fn, _arg)                                                                 \
	__attribute__((cleanup(_defer_run))) defer_entry_t _DEFER_CONCAT(_defer_, __LINE__) = { \
		.fn = (void *)(_fn), .arg = (void *)(uintptr_t)(_arg), .has_arg = 1                 \
	}

#define _GET_DEFER_MACRO(_1, _2, NAME, ...) NAME
#define defer(...)							_GET_DEFER_MACRO(__VA_ARGS__, _DEFER_2, _DEFER_1, ~)(__VA_ARGS__)

// TODO: impl UnwrapWithContext and TryWithContext
