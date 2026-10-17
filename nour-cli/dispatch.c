#include "dispatch.h"
#include "str.h"
#include "theme.h"
#include "ui.h"

#include <nour/config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Flag *flag_find_sv(Flag *flags, StrView name)
{
	for (i32 i = 0; flags && flags[i].name; ++i) {
		if (sv_eq_cstr(name, flags[i].name))
			return &flags[i];
	}
	return NULL;
}

static Flag *flag_find(Flag *flags, const char *name)
{
	if (!name)
		return NULL;
	return flag_find_sv(flags, sv_fromc(name));
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

static usize levenshtein_dist_sv(StrView s1, StrView s2)
{
	usize l1 = s1.len;
	usize l2 = s2.len;
	if (l1 == 0)
		return l2;
	if (l2 == 0)
		return l1;

	usize d[64][64];
	if (l1 >= 64)
		l1 = 63;
	if (l2 >= 64)
		l2 = 63;

	for (usize i = 0; i <= l1; ++i)
		d[i][0] = i;
	for (usize j = 0; j <= l2; ++j)
		d[0][j] = j;

	for (usize i = 1; i <= l1; ++i) {
		for (usize j = 1; j <= l2; ++j) {
			usize cost =
				(tolower((unsigned char)s1.data[i - 1]) == tolower((unsigned char)s2.data[j - 1])) ?
					0 :
					1;
			usize del = d[i - 1][j] + 1;
			usize ins = d[i][j - 1] + 1;
			usize sub = d[i - 1][j - 1] + cost;
			usize min = del < ins ? del : ins;
			d[i][j] = sub < min ? sub : min;
		}
	}
	return d[l1][l2];
}

static Command *find_closest_cmd(Command **cmds, const char *name)
{
	if (!cmds || !name)
		return NULL;
	Command *best = NULL;
	usize best_dist = (usize)-1;
	StrView name_sv = sv_fromc(name);

	for (i32 i = 0; cmds[i]; ++i) {
		usize d = levenshtein_dist_sv(name_sv, sv_fromc(cmds[i]->name));
		if (d < best_dist) {
			best_dist = d;
			best = cmds[i];
		}
	}

	usize threshold = name_sv.len <= 3 ? 1 : name_sv.len <= 6 ? 2 : 3;
	if (best_dist <= threshold)
		return best;
	return NULL;
}

static Flag *find_closest_flag_sv(Flag *flags, StrView name)
{
	if (!flags || sv_empty(name))
		return NULL;
	Flag *best = NULL;
	usize best_dist = (usize)-1;

	for (i32 i = 0; flags[i].name; ++i) {
		usize d = levenshtein_dist_sv(name, sv_fromc(flags[i].name));
		if (d < best_dist) {
			best_dist = d;
			best = &flags[i];
		}
	}

	usize threshold = name.len <= 3 ? 1 : name.len <= 6 ? 2 : 3;
	if (best_dist <= threshold)
		return best;
	return NULL;
}

static void print_flags(Flag *flags, const char *header)
{
	if (!flags || !flags[0].name)
		return;
	printf("\n%s\n", header);

	for (i32 i = 0; flags[i].name; ++i) {
		Flag *f = &flags[i];
		const char *hint = f->hint;
		if (!hint) {
			if (f->type == FLAG_STR) {
				if (strcmp(f->name, "dir") == 0)
					hint = "<path>";
				else if (strcmp(f->name, "target") == 0)
					hint = "<name>";
				else if (strcmp(f->name, "profile") == 0)
					hint = "<name>";
				else
					hint = "<value>";
			} else if (f->type == FLAG_INT) {
				hint = "<int>";
			} else {
				hint = "";
			}
		}

		char visible[64];
		if (f->shorthand) {
			if (hint[0]) {
				snprintf(visible, sizeof(visible), "-%c, --%s %s", f->shorthand, f->name, hint);
			} else {
				snprintf(visible, sizeof(visible), "-%c, --%s", f->shorthand, f->name);
			}
		} else {
			if (hint[0]) {
				snprintf(visible, sizeof(visible), "    --%s %s", f->name, hint);
			} else {
				snprintf(visible, sizeof(visible), "    --%s", f->name);
			}
		}

		usize len = strlen(visible);
		i32 pad = 24 - (i32)len;
		if (pad < 2)
			pad = 2;

		if (f->shorthand) {
			if (hint[0]) {
				printf(
					"  %s-%c, --%s%s %s%s%s%*s%s", th.teal, f->shorthand, f->name, th.reset,
					th.slate, hint, th.reset, pad, "", f->usage ? f->usage : ""
				);
			} else {
				printf(
					"  %s-%c, --%s%s%*s%s", th.teal, f->shorthand, f->name, th.reset, pad, "",
					f->usage ? f->usage : ""
				);
			}
		} else {
			if (hint[0]) {
				printf(
					"      %s--%s%s %s%s%s%*s%s", th.teal, f->name, th.reset, th.slate, hint,
					th.reset, pad, "", f->usage ? f->usage : ""
				);
			} else {
				printf(
					"      %s--%s%s%*s%s", th.teal, f->name, th.reset, pad, "",
					f->usage ? f->usage : ""
				);
			}
		}

		switch (f->type) {
		case FLAG_BOOL:
			if (f->val.b)
				printf(" %s(default: true)%s", th.slate, th.reset);
			break;
		case FLAG_STR:
			if (f->val.s && *f->val.s)
				printf(" %s(default: \"%s\")%s", th.slate, f->val.s, th.reset);
			break;
		case FLAG_INT:
			if (f->val.i)
				printf(" %s(default: %d)%s", th.slate, f->val.i, th.reset);
			break;
		}
		if (f->required)
			printf(" %s[required]%s", th.crimson, th.reset);
		printf("\n");
	}
}

static void print_global_flags(void)
{
	printf("\nFLAGS\n");
	printf("  %s-h, --help%s%*sShow help\n", th.teal, th.reset, 14, "");
	printf("  %s-V, --version%s%*sPrint version\n", th.teal, th.reset, 11, "");
}

static void help_cli(CLI *cli)
{
	ui_header(cli->version ? cli->version : NOUR_VERSION);

	printf("USAGE\n");
	printf("  $ %s [COMMAND] [OPTIONS]\n\n", cli->name);

	printf("COMMANDS\n");
	for (i32 i = 0; i < DISPATCH_MAX_COMMANDS && cli->commands[i]; ++i) {
		Command *cmd = cli->commands[i];
		char label[64];
		if (cmd->alias) {
			snprintf(label, sizeof(label), "%s, %s", cmd->name, cmd->alias);
		} else {
			snprintf(label, sizeof(label), "%s", cmd->name);
		}
		usize len = strlen(label);
		i32 pad = 24 - (i32)len;
		if (pad < 2)
			pad = 2;
		printf("  %s%s%s%*s%s\n", th.white, label, th.reset, pad, "", cmd->usage ? cmd->usage : "");
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
			char label[64];
			snprintf(label, sizeof(label), "%s", cmd->subcommands[i]->name);
			usize len = strlen(label);
			i32 pad = 24 - (i32)len;
			if (pad < 2)
				pad = 2;
			printf(
				"  %s%s%s%*s%s\n", th.white, label, th.reset, pad, "",
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
	ctx->n_args = 0;
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
			StrView rest = sv_sub(sv_fromc(arg), 2, strlen(arg) - 2);
			StrView key_sv, inline_val_sv;
			bool	has_eq = sv_split(rest, '=', &key_sv, &inline_val_sv);

			Flag *f = flag_find_sv(cmd->flags, key_sv);
			if (!f) {
				return Err("unknown flag '--%.*s'", (int)key_sv.len, key_sv.data);
			}
			f->_set = true;

			if (f->type == FLAG_BOOL) {
				if (has_eq) {
					f->val.b =
						(sv_eq_cstr(inline_val_sv, "true") || sv_eq_cstr(inline_val_sv, "1"));
				} else {
					f->val.b = true;
				}
				++i;
				continue;
			}

			const char *val = NULL;
			if (has_eq) {
				val = inline_val_sv.data;
			} else {
				if (i + 1 >= argc || argv[i + 1][0] == '-') {
					return Err("'--%.*s' requires a value", (int)key_sv.len, key_sv.data);
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
				if (e == val || *e != '\0') {
					return Err("'--%.*s' expects integer", (int)key_sv.len, key_sv.data);
				}
				break;
			}
			case FLAG_BOOL:
				break;
			}
			++i;
		} else if (arg[0] == '-' && arg[1] && arg[1] != '-') {
			i32 j = 1;
			while (arg[j]) {
				char sc = arg[j];
				Flag *f = flag_find_short(cmd->flags, sc);
				if (!f)
					return Err(ERR_UNKNOWN_SHORT_FLAG, sc);
				f->_set = true;

				if (f->type == FLAG_BOOL) {
					f->val.b = true;
					j++;
					continue;
				}

				const char *val;
				if (arg[j + 1]) {
					val = arg + j + 1;
					j = (i32)strlen(arg);
				} else {
					if (i + 1 >= argc || argv[i + 1][0] == '-') {
						return Err(ERR_SHORT_REQUIRES_VAL, sc);
					}
					val = argv[++i];
					j = (i32)strlen(arg);
				}

				switch (f->type) {
				case FLAG_STR:
					f->val.s = val;
					break;
				case FLAG_INT: {
					char *e;
					f->val.i = (i32)strtol(val, &e, 10);
					if (e == val || *e != '\0')
						return Err(ERR_SHORT_EXPECTS_INT, sc);
					break;
				}
				case FLAG_BOOL:
					break;
				}
			}
			++i;
		} else {
			if (ctx->n_args >= MAX_ARGS)
				return Err(ERR_TOO_MANY_ARGS);
			ctx->args[ctx->n_args++] = arg;
			++i;
		}
	}

	for (i32 i = 0; i < DISPATCH_MAX_FLAGS && cmd->flags[i].name; ++i) {
		if (cmd->flags[i].required && !cmd->flags[i]._set) {
			return Err(ERR_MISSING_REQ_FLAG, cmd->flags[i].name);
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

	Command *cmd = NULL;
	Command **search_in = cli->commands;
	i32 arg_start = 1;

	while (arg_start < argc) {
		const char *tok = argv[arg_start];
		if (tok[0] == '-')
			break;
		Command *found = find_cmd(search_in, tok);
		if (!found)
			break;
		cmd = found;
		search_in = cmd->subcommands;
		arg_start++;
	}

	if (!cmd) {
		ui_error("unknown command '%s'", argv[1]);
		Command *best = find_closest_cmd(cli->commands, argv[1]);
		if (best) {
			ui_info("Did you mean '%s'?", best->name);
		}
		ui_info("Run '%s --help' to see all available commands.", cli->name);
		return 1;
	}
	if (!cmd->action) {
		help_cmd(cli, cmd);
		return 0;
	}

	Context ctx = {.cmd = cmd, .cli = cli};
	bool	show_help = false;
	Result	res = parse_flags(cmd, argc, argv, arg_start, &ctx, &show_help);
	defer(result_free, &res);
	if (!res.ok) {
		ui_error("%s", res.error);
		StrView err_sv = sv_fromc(res.error);
		if (sv_starts(err_sv, SV("unknown flag '--"))) {
			StrView rest = sv_sub(err_sv, 16, err_sv.len - 16);
			StrView flag_name;
			if (sv_split(rest, '\'', &flag_name, NULL)) {
				Flag *best_flag = find_closest_flag_sv(cmd->flags, flag_name);
				if (best_flag) {
					ui_info("Did you mean '--%s'?", best_flag->name);
				}
			}
		}
		ui_info("Run '%s %s --help' for flag usage.", cli->name, cmd->name);
		return 1;
	}

	if (show_help) {
		help_cmd(cli, cmd);
		return 0;
	}

	res = cmd->action(&ctx);
	if (!res.ok) {
		ui_error("%s", res.error);
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
