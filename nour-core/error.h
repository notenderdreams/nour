#pragma once

#include "types.h"

/* Error codes */
typedef enum {
	ERR_OK = 0,
	ERR_UNKNOWN,
	ERR_FAILED,
	ERR_TARGET_EXIT,
	ERR_PROC_EXIT,
	ERR_CMD_STATUS,
	ERR_OUT_OF_MEMORY,
	ERR_INDEX_BOUNDS,
	ERR_TIMEOUT,
	ERR_DIR_CREATE,
	ERR_FILE_OPEN,
	ERR_FILE_WRITE,
	ERR_FILE_REMOVE,
	ERR_FILE_NOT_FOUND,
	ERR_FILE_READ,
	ERR_NO_PROJECT,
	ERR_UNCLOSED_ARRAY,
	ERR_DEPTH_LIMIT,
	ERR_PARSE_LINE,
	ERR_COMPILE_FAIL,
	ERR_DLOPEN,
	ERR_DLSYM,
	ERR_NULL_PROJECT,
	ERR_TARGET_NOT_FOUND,
	ERR_TARGET_NO_SOURCES,
	ERR_TARGET_NO_ROOT,
	ERR_TARGET_NO_MATCH,
	ERR_TARGET_ZERO_SOURCES,
	ERR_BUILD_FAILED,
	ERR_LIB_UNIMPLEMENTED,
	ERR_UNKNOWN_TARGET,
	ERR_NOT_EXECUTABLE,
	ERR_NO_EXE_TARGET,
	ERR_MODULE_NO_MATCH,
	ERR_UNKNOWN_FLAG,
	ERR_FLAG_REQUIRES_VAL,
	ERR_FLAG_EXPECTS_INT,
	ERR_UNKNOWN_SHORT_FLAG,
	ERR_SHORT_REQUIRES_VAL,
	ERR_SHORT_EXPECTS_INT,
	ERR_TOO_MANY_ARGS,
	ERR_MISSING_REQ_FLAG,
	ERR_INVALID_NAME,
	ERR_DIR_EXISTS,
	ERR_FILE_EXISTS,
	ERR_GETCWD,
	ERR_CIRCULAR_DEP,
	ERR_INVALID_DEP,
	ERR_INVALID_DECL,
	ERR_DUPLICATE_TARGET,
	ERR_DEP_DEPTH,
	ERR_AMBIGUOUS_EXEC,
	ERR_MAX,
} ErrorCode;

/* Error messages array (indexed by error code) */
__attribute__((unused)) static const char *const messages[] = {
	[ERR_OK] = "success",
	[ERR_UNKNOWN] = "something went wrong: %s",
	[ERR_FAILED] = "%s",
	[ERR_TARGET_EXIT] = "target '%s' exited with code %d",
	[ERR_PROC_EXIT] = "process '%s' exited with code %d",
	[ERR_CMD_STATUS] = "command failed with exit status %d",
	[ERR_OUT_OF_MEMORY] = "out of memory: failed to allocate %d bytes",
	[ERR_INDEX_BOUNDS] = "index %d out of bounds (max %d)",
	[ERR_TIMEOUT] = "operation timed out after %d ms",
	[ERR_DIR_CREATE] = "failed to create directory '%s'",
	[ERR_FILE_OPEN] = "failed to open file '%s': %s",
	[ERR_FILE_WRITE] = "failed to write content to '%s'",
	[ERR_FILE_REMOVE] = "failed to remove directory '%s'",
	[ERR_FILE_NOT_FOUND] = "could not find '%s'",
	[ERR_FILE_READ] = "failed to read file '%s': %s",
	[ERR_NO_PROJECT] = "No Project declaration found in '%s'",
	[ERR_UNCLOSED_ARRAY] = "Unclosed array in '%s'",
	[ERR_DEPTH_LIMIT] = "Include depth limit exceeded (max %d) while processing '%s'",
	[ERR_PARSE_LINE] = "syntax error in '%s' at line %d: %s",
	[ERR_COMPILE_FAIL] = "Failed to compile '%s' into shared library",
	[ERR_DLOPEN] = "dlopen failed: %s",
	[ERR_DLSYM] = "dlsym failed: %s",
	[ERR_NULL_PROJECT] = "nour_get_project returned NULL",
	[ERR_TARGET_NOT_FOUND] = "Target '%s' not found in project '%s'",
	[ERR_TARGET_NO_SOURCES] = "Target '%s' has no sources specified",
	[ERR_TARGET_NO_ROOT] = "Target '%s': root file '%s' not found",
	[ERR_TARGET_NO_MATCH] = "Target '%s': no files found matching '%s'",
	[ERR_TARGET_ZERO_SOURCES] = "Target '%s': zero source files resolved",
	[ERR_BUILD_FAILED] = "Build failed for target '%s'",
	[ERR_LIB_UNIMPLEMENTED] = "Target '%s': library builds are not yet implemented",
	[ERR_UNKNOWN_TARGET] = "Target '%s': unknown target kind",
	[ERR_NOT_EXECUTABLE] = "target '%s' is not an executable",
	[ERR_NO_EXE_TARGET] = "no executable target found to run in project '%s'",
	[ERR_MODULE_NO_MATCH] = "Module '%s': no files found matching '%s'",
	[ERR_UNKNOWN_FLAG] = "unknown flag '--%s'",
	[ERR_FLAG_REQUIRES_VAL] = "'--%s' requires a value",
	[ERR_FLAG_EXPECTS_INT] = "'--%s' expects integer",
	[ERR_UNKNOWN_SHORT_FLAG] = "unknown flag '-%c'",
	[ERR_SHORT_REQUIRES_VAL] = "'-%c' requires a value",
	[ERR_SHORT_EXPECTS_INT] = "'-%c' expects integer",
	[ERR_TOO_MANY_ARGS] = "too many arguments",
	[ERR_MISSING_REQ_FLAG] = "required flag '--%s' not provided",
	[ERR_INVALID_NAME] = "invalid project name '%s'. Must be a valid C identifier",
	[ERR_DIR_EXISTS] = "directory '%s' already exists",
	[ERR_FILE_EXISTS] = "'%s' already exists in current directory",
	[ERR_GETCWD] = "failed to get current working directory",
	[ERR_CIRCULAR_DEP] = "circular dependency detected: %s",
	[ERR_INVALID_DEP] = "target '%s' has invalid dependency '%s'",
	[ERR_INVALID_DECL] = "invalid declaration name '%s': must be a valid C identifier",
	[ERR_DUPLICATE_TARGET] = "duplicate target name '%s'",
	[ERR_DEP_DEPTH] = "dependency depth limit exceeded (max %d) at target '%s'",
	[ERR_AMBIGUOUS_EXEC] = "multiple executable targets in project '%s'; specify one with --target",
};

#define error_messages messages

static inline Result _err_code(int code, ...)
{
	char fallback[64];
	const char *fmt = NULL;
	if (code < 0 || (size_t)code >= sizeof(messages) / sizeof(messages[0]) || !messages[code]) {
		snprintf(
			fallback, sizeof(fallback), "something went wrong: unknown error occurred (%d)", code
		);
		fmt = fallback;
	} else {
		fmt = messages[code];
	}

	va_list args;
	va_start(args, code);
	Result res = _vresult_err(fmt, args);
	va_end(args);
	return res;
}

#define Err(first, ...)                                                              \
	_Generic((first), char *: _err_fmt, const char *: _err_fmt, default: _err_code)( \
		first, ##__VA_ARGS__                                                         \
	)

#define err(...) Err(__VA_ARGS__)
