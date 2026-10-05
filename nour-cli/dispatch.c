#include "dispatch.h"
#include "ui.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Flag *flag_find(Flag *flags, const char *name)
{
	for (i32 i = 0; flags && flags[i].name; ++i) {
		if (strcmp(flags[i].name, name) == 0)
			return &flags[i];
	}
	return NULL;
}

static Flag *flag_find_short(Flag *flags, char c)
{
	for (i32 i = 0; flags && flags[i].name; ++i) {
		if (flags[i].shorthand == c)
			return &flags[i];
	}
	return NULL;
}

static Command *find_cmd(Command **cmds, const char *name)
{
	for (i32 i = 0; cmds && cmds[i]; ++i) {
		if (strcmp(cmds[i]->name, name) == 0)
			return cmds[i];
		if (cmds[i]->alias && strcmp(cmds[i]->alias, name) == 0)
			return cmds[i];
	}
	return NULL;
}

static void print_flags(Flag *flags, const char *header)
{
	if (!flags || !flags[0].name)
		return;
	printf("\n%s\n", header);

	for (i32 i = 0; flags[i].name; ++i) {
		Flag *f		= &flags[i];
		char  sh[6] = "    ";
		if (f->shorthand)
			snprintf(sh, sizeof(sh), "-%c, ", f->shorthand);

		const char *type_hint = f->type == FLAG_STR ? " <string>" :
								f->type == FLAG_INT ? " <int>" :
													  "";

		printf(
			"   %s%s--%-14s%s  %s%s", sh, th.teal, f->name, th.reset, f->usage ? f->usage : "",
			type_hint
		);

		switch (f->type) {
		case FLAG_BOOL:
			if (f->val.b)
				printf(" (default: true)");
			break;
		case FLAG_STR:
			if (f->val.s && *f->val.s)
				printf(" (default: \"%s\")", f->val.s);
			break;
		case FLAG_INT:
			if (f->val.i)
				printf(" (default: %d)", f->val.i);
			break;
		}
		if (f->required)
			printf(" [required]");
		printf("\n");
	}
}

static void print_global_flags(void)
{
	printf("\nFLAGS\n");
	printf("   -h, %s--help%s          Show help\n", th.teal, th.reset);
	printf("   -V, %s--version%s       Print version\n", th.teal, th.reset);
}

static void help_cli(CLI *cli)
{
	ui_header(cli->version ? cli->version : NOUR_VERSION);

	printf("USAGE\n");
	printf("  $ %s [COMMAND] [OPTIONS]\n\n", cli->name);

	printf("COMMANDS\n");
	for (i32 i = 0; i < DISPATCH_MAX_COMMANDS && cli->commands[i]; ++i) {
		Command *cmd = cli->commands[i];
		char	 label[64];
		if (cmd->alias) {
			snprintf(label, sizeof(label), "%s, %s", cmd->name, cmd->alias);
		} else {
			snprintf(label, sizeof(label), "%s", cmd->name);
		}
		printf("  %s%-16s%s %s\n", th.white, label, th.reset, cmd->usage ? cmd->usage : "");
	}
	print_global_flags();
	printf("\n");
}

static void help_cmd(CLI *cli, Command *cmd)
{
	printf(
		"\n%s%s%s — %s\n\n", th.bold, cmd->name, th.reset,
		cmd->description ? cmd->description : (cmd->usage ? cmd->usage : "")
	);
	printf("USAGE\n");
	printf("  $ %s %s%s%s [options]\n", cli->name, th.teal, cmd->name, th.reset);

	if (cmd->subcommands[0]) {
		printf("\nSUBCOMMANDS\n");
		for (i32 i = 0; cmd->subcommands[i]; ++i) {
			printf(
				"  %s%-16s%s %s\n", th.white, cmd->subcommands[i]->name, th.reset,
				cmd->subcommands[i]->usage ? cmd->subcommands[i]->usage : ""
			);
		}
	}
	print_flags(cmd->flags, "OPTIONS");
	print_global_flags();
	printf("\n");
}

static Result
parse_flags(Command *cmd, i32 argc, char **argv, i32 start, Context *ctx, bool *show_help)
{
	*show_help = false;
	for (i32 i = 0; i < DISPATCH_MAX_FLAGS && cmd->flags[i].name; ++i) {
		cmd->flags[i]._set = false;
	}
	ctx->n_args		 = 0;
	ctx->n_forwarded = 0;

	for (i32 i = start; i < argc;) {
		const char *arg = argv[i];

		if (strcmp(arg, "--") == 0) {
			++i;
			while (i < argc && ctx->n_forwarded < MAX_ARGS) {
				ctx->forwarded_args[ctx->n_forwarded++] = argv[i++];
			}
			break;
		}

		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			*show_help = true;
			return Ok(NULL);
		}

		if (arg[0] == '-' && arg[1] == '-') {
			const char *key = arg + 2;
			const char *eq	= strchr(key, '=');
			char		buf[128];
			const char *inline_val = NULL;

			if (eq) {
				usize len = (usize)(eq - key);
				if (len >= sizeof(buf))
					return Err("flag name too long");
				memcpy(buf, key, len);
				buf[len]   = '\0';
				inline_val = eq + 1;
			} else {
				if (strlen(key) >= sizeof(buf))
					return Err("flag name too long");
				strncpy(buf, key, sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
			}

			Flag *f = flag_find(cmd->flags, buf);
			if (!f)
				return Err("unknown flag '--%s'", buf);
			f->_set = true;

			if (f->type == FLAG_BOOL) {
				f->val.b = inline_val ?
							   (strcmp(inline_val, "true") == 0 || strcmp(inline_val, "1") == 0) :
							   true;
				++i;
				continue;
			}

			const char *val = inline_val;
			if (!val) {
				if (i + 1 >= argc || argv[i + 1][0] == '-') {
					return Err("'--%s' requires a value", buf);
				}
				val = argv[++i];
			}

			switch (f->type) {
			case FLAG_STR:
				f->val.s = val;
				break;
			case FLAG_INT: {
				char *e;
				f->val.i = (i32)strtol(val, &e, 10);
				if (e == val || *e != '\0')
					return Err("'--%s' expects integer", buf);
				break;
			}
			case FLAG_BOOL:
				break;
			}
			++i;
		} else if (arg[0] == '-' && arg[1] && arg[1] != '-') {
			i32 j = 1;
			while (arg[j]) {
				char  sc = arg[j];
				Flag *f	 = flag_find_short(cmd->flags, sc);
				if (!f)
					return Err("unknown flag '-%c'", sc);
				f->_set = true;

				if (f->type == FLAG_BOOL) {
					f->val.b = true;
					j++;
					continue;
				}

				const char *val;
				if (arg[j + 1]) {
					val = arg + j + 1;
					j	= (i32)strlen(arg);
				} else {
					if (i + 1 >= argc || argv[i + 1][0] == '-') {
						return Err("'-%c' requires a value", sc);
					}
					val = argv[++i];
					j	= (i32)strlen(arg);
				}

				switch (f->type) {
				case FLAG_STR:
					f->val.s = val;
					break;
				case FLAG_INT: {
					char *e;
					f->val.i = (i32)strtol(val, &e, 10);
					if (e == val || *e != '\0')
						return Err("'-%c' expects integer", sc);
					break;
				}
				case FLAG_BOOL:
					break;
				}
			}
			++i;
		} else {
			if (ctx->n_args >= MAX_ARGS)
				return Err("too many arguments");
			ctx->args[ctx->n_args++] = arg;
			++i;
		}
	}

	for (i32 i = 0; i < DISPATCH_MAX_FLAGS && cmd->flags[i].name; ++i) {
		if (cmd->flags[i].required && !cmd->flags[i]._set) {
			return Err("required flag '--%s' not provided", cmd->flags[i].name);
		}
	}
	return Ok(NULL);
}

i32 dispatch(CLI *cli, i32 argc, char **argv)
{
	theme_init();

	if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
		strcmp(argv[1], "help") == 0) {
		if (argc > 2) {
			Command *sub = find_cmd(cli->commands, argv[2]);
			if (sub) {
				help_cmd(cli, sub);
				return 0;
			}
		}
		help_cli(cli);
		return 0;
	}
	if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0 ||
		strcmp(argv[1], "-v") == 0) {
		printf("%s v%s\n", cli->name, cli->version ? cli->version : NOUR_VERSION);
		return 0;
	}

	Command	 *cmd		= NULL;
	Command **search_in = cli->commands;
	i32		  arg_start = 1;

	while (arg_start < argc) {
		const char *tok = argv[arg_start];
		if (tok[0] == '-')
			break;
		Command *found = find_cmd(search_in, tok);
		if (!found)
			break;
		cmd		  = found;
		search_in = cmd->subcommands;
		arg_start++;
	}

	if (!cmd) {
		ui_error("unknown command '%s'", argv[1]);
		ui_info("Run '%s --help' to see all available commands.", cli->name);
		return 1;
	}
	if (!cmd->action) {
		help_cmd(cli, cmd);
		return 0;
	}

	Context ctx		  = { .cmd = cmd, .cli = cli };
	bool	show_help = false;
	Result	res		  = parse_flags(cmd, argc, argv, arg_start, &ctx, &show_help);
	if (!res.ok) {
		ui_error("%s", res.error);
		ui_info("Run '%s %s --help' for flag usage.", cli->name, cmd->name);
		result_free(&res);
		return 1;
	}

	if (show_help) {
		help_cmd(cli, cmd);
		return 0;
	}

	res = cmd->action(&ctx);
	if (!res.ok) {
		ui_error("%s", res.error);
		result_free(&res);
		return 1;
	}

	return 0;
}

bool dispatch_flag_bool(Context *ctx, const char *name, bool fallback)
{
	Flag *f = flag_find(ctx->cmd->flags, name);
	if (!f)
		return fallback;
	return f->_set ? f->val.b : fallback;
}

const char *dispatch_flag_str(Context *ctx, const char *name, const char *fallback)
{
	Flag *f = flag_find(ctx->cmd->flags, name);
	if (!f)
		return fallback;
	if (f->_set && f->val.s)
		return f->val.s;
	if (fallback && fallback[0])
		return fallback;
	return f->val.s ? f->val.s : fallback;
}

i32 dispatch_flag_int(Context *ctx, const char *name, i32 fallback)
{
	Flag *f = flag_find(ctx->cmd->flags, name);
	if (!f)
		return fallback;
	return f->_set ? f->val.i : fallback;
}
