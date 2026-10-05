#pragma once
#include "types.h"

#define DISPATCH_MAX_COMMANDS 32
#define DISPATCH_MAX_FLAGS	  32
#define MAX_ARGS			  64

#define FLAG_END { .name = NULL }
#define CMD_END	 NULL
#define NO_SHORT '\0'

typedef enum {
	FLAG_BOOL,
	FLAG_STR,
	FLAG_INT,
} FlagType;

typedef struct {
	const char *name;
	char		shorthand;
	FlagType	type;
	union {
		bool		b;
		const char *s;
		i32			i;
	} val;
	const char *usage;
	bool		required;
	bool		_set;
} Flag;

typedef struct Command Command;
typedef struct CLI	   CLI;

typedef struct {
	Command	   *cmd;
	CLI		   *cli;
	const char *args[MAX_ARGS];
	u32			n_args;
	const char *forwarded_args[MAX_ARGS];
	u32			n_forwarded;
} Context;

struct Command {
	const char *name;
	const char *alias;
	const char *usage;
	const char *description;
	Command	   *subcommands[DISPATCH_MAX_COMMANDS];
	Flag		flags[DISPATCH_MAX_FLAGS];
	Result (*action)(Context *ctx); // Action returns Result
};

struct CLI {
	const char *name;
	const char *version;
	const char *description;
	Command	   *commands[DISPATCH_MAX_COMMANDS];
};

bool		dispatch_flag_bool(Context *ctx, const char *name, bool fallback);
const char *dispatch_flag_str(Context *ctx, const char *name, const char *fallback);
i32			dispatch_flag_int(Context *ctx, const char *name, i32 fallback);

#define flag_bool(ctx, name) dispatch_flag_bool((ctx), (name), false)

// Single C11 _Generic macro: type is inferred automatically from fallback!
#define flag(ctx, name, fallback)                                                            \
	_Generic(                                                                                \
		(fallback),                                                                          \
		bool: dispatch_flag_bool((ctx), (name), (bool)(uintptr_t)(fallback)),                \
		const char *: dispatch_flag_str((ctx), (name), (const char *)(uintptr_t)(fallback)), \
		char *: dispatch_flag_str((ctx), (name), (const char *)(uintptr_t)(fallback)),       \
		default: dispatch_flag_int((ctx), (name), (i32)(intptr_t)(fallback))                 \
	)

// Returns explicit exit code (0 = success, 1 = failure)
i32 dispatch(CLI *cli, i32 argc, char **argv);
