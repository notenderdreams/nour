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

static inline Result Ok(void *value)
{
	return (Result){
		.ok	   = true,
		.value = value,
		.error = NULL,
	};
}

__attribute__((format(printf, 1, 2))) static inline Result Err(const char *fmt, ...)
{
	va_list args, args_copy;
	va_start(args, fmt);
	va_copy(args_copy, args);

	i32 len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *buf = NULL;
	if (len >= 0) {
		buf = malloc(len + 1);
		if (buf) {
			vsnprintf(buf, len + 1, fmt, args_copy);
		}
	}
	va_end(args_copy);

	return (Result){
		.ok	   = false,
		.value = NULL,
		.error = buf,
	};
}

static inline void result_free(Result *res)
{
	if (res && !res->ok && res->error) {
		free((void *)res->error);
		res->error = NULL;
	}
}

static inline bool report(Result *res)
{
	if (res->ok)
		return false;

	fprintf(stderr, "[ERROR] %s\n", res->error ? res->error : "unknown error");
	result_free(res);
	return true;
}

#define TRY(expr)           \
	do {                    \
		Result _r = (expr); \
		if (!_r.ok)         \
			return _r;      \
	} while (0)
