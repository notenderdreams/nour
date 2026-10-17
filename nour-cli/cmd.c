#include "cmd.h"
#include "arena.h"
#include "builder.h"
#include "fs.h"
#include "loader.h"
#include "parser.h"
#include "str.h"
#include "templates.h"
#include "theme.h"
#include "ui.h"
#include "utils.h"

#include <nour/config.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUILD_FLAGS                                                                             \
	{                                                                                           \
		{"dir", 'C', FLAG_STR, .val.s = ".", "project directory", "<path>"},                    \
		{"target", 't', FLAG_STR, .val.s = "", "specific build target", "<name>"},              \
		{"profile", 'p', FLAG_STR, .val.s = "", "build profile (e.g. debug, release, staging)", \
		 "<name>"},                                                                             \
		{"release", 'r', FLAG_BOOL, .val.b = false, "shorthand for --profile release"},         \
		FLAG_END,                                                                               \
	}

Command new_cmd = {
	.name	= "new",
	.alias	= "n",
	.usage	= "Create a new project directory",
	.action = cmd_new,
};

Command init_cmd = {
	.name	= "init",
	.alias	= "i",
	.usage	= "Initialize current directory as a project",
	.action = cmd_init,
};

Command build_cmd = {
	.name	= "build",
	.alias	= "b",
	.usage	= "Compile the project",
	.action = cmd_build,
	.flags	= BUILD_FLAGS,
};

Command run_cmd = {
	.name  = "run",
	.alias = "r",
	.usage = "Build and run the executable binary",
	.description =
		"Builds then runs the target binary.\nPass '--' to forward arguments to the executable.",
	.action = cmd_run,
	.flags	= BUILD_FLAGS,
};

Command clean_cmd = {
	.name	= "clean",
	.usage	= "Remove build artifacts and build directory",
	.action = cmd_clean,
	.flags	= {
		 {"dir", 'C', FLAG_STR, .val.s = ".", "project directory", "<path>"},
		 FLAG_END,
	 },
};

static Result scaffold_project(const char *name, const char *base_dir)
{
	arena_scratch(a, 4096);

	const char *src_dir = rnstr_sprintf(&a, "%s/src", base_dir);
	const char *nour_file = rnstr_sprintf(&a, "%s/project.nour", base_dir);
	const char *main_file = rnstr_sprintf(&a, "%s/src/main.c", base_dir);
	const char *gitignore_file = rnstr_sprintf(&a, "%s/.gitignore", base_dir);

	Try(fs_create_dir(src_dir));
	ui_created(src_dir);

	const char *manifest = rnstr_sprintf(&a, MANIFEST_TEMPLATE, name, name, name);
	Try(fs_write_file(nour_file, manifest));
	ui_created(nour_file);

	const char *main_src = rnstr_sprintf(&a, MAIN_TEMPLATE, name);
	Try(fs_write_file(main_file, main_src));
	ui_created(main_file);

	Try(fs_write_file(gitignore_file, GITIGNORE_TEMPLATE));
	ui_created(gitignore_file);

	return Ok(NULL);
}

Result cmd_new(Context *ctx)
{
	if (ctx->n_args < 1) {
		return Err("'new' requires a project name. Usage: %s new <name>", ctx->cli->name);
	}

	const char *project_name = ctx->args[0];
	if (!is_valid_name(project_name)) {
		return Err(ERR_INVALID_NAME, project_name);
	}

	if (fs_dir_exists(project_name)) {
		return Err(ERR_DIR_EXISTS, project_name);
	}

	Try(fs_create_dir(project_name));
	ui_created(project_name);

	Try(scaffold_project(project_name, project_name));
	printf("\n");
	ui_success("Project '%s' created successfully!", project_name);
	ui_next_steps(project_name, "nour run");
	return Ok(NULL);
}

Result cmd_init(Context *ctx)
{
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd)))
		return Err(ERR_GETCWD);

	char *project_name = strrchr(cwd, '/');
	project_name = project_name ? project_name + 1 : cwd;

	if (ctx->n_args > 0) {
		project_name = (char *)ctx->args[0];
	}

	if (!is_valid_name(project_name)) {
		return Err(ERR_INVALID_NAME, project_name);
	}

	if (fs_file_exists("project.nour")) {
		return Err(ERR_FILE_EXISTS, "project.nour");
	}

	Try(scaffold_project(project_name, "."));
	printf("\n");
	ui_success("Initialized Nour project in current directory.");
	ui_next_steps(NULL, "nour run");
	return Ok(NULL);
}

static f64 time_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (f64)ts.tv_sec * 1000.0 + (f64)ts.tv_nsec / 1000000.0;
}

static const char *resolve_profile(Context *ctx)
{
	const char *p = flag(ctx, "profile", "");
	if (p && p[0])
		return p;

	bool is_release = flag(ctx, "release", false);
	return is_release ? "release" : "debug";
}

static Result execute_project_build(
	const char *project_dir, const char *target_name, const char *profile, char *out_bin,
	usize out_bin_cap
) {
	arena_scratch(a, 8192);

	const char *nour_path = rnstr_sprintf(&a, "%s/project.nour", project_dir);
	const char *dot_nour_dir = rnstr_sprintf(&a, "%s/.nour", project_dir);
	const char *c_path = rnstr_sprintf(&a, "%s/.nour/project.nour.c", project_dir);
	const char *so_path = rnstr_sprintf(&a, "%s/.nour/libnour.so", project_dir);

	if (!fs_file_exists(nour_path)) {
		return Err(ERR_FILE_NOT_FOUND, nour_path);
	}

	f64 total_start = time_ms();
	Try(fs_create_dir(dot_nour_dir));

	f64 t0 = time_ms();

	// Preprocess to a scratch file and only replace the manifest .c when its
	// content actually changed; likewise, skip recompiling the shared object
	// when it is newer than the manifest source. This keeps the dlopen cache
	// warm so repeated commands don't pay the compile+link tax every time.
	const char *tmp_c_path = rnstr_sprintf(&a, "%s.tmp", c_path);
	Try(preprocess(nour_path, tmp_c_path));

	bool c_changed = true;
	{
		char buf_a[4096], buf_b[4096];
		FILE *fa = fopen(c_path, "rb");
		FILE *fb = fopen(tmp_c_path, "rb");
		if (fa) {
			defer(fclose, fa);
			if (fb) {
				defer(fclose, fb);
				bool equal = true;
				for (;;) {
					usize na = fread(buf_a, 1, sizeof(buf_a), fa);
					usize nb = fread(buf_b, 1, sizeof(buf_b), fb);
					if (na != nb || memcmp(buf_a, buf_b, na) != 0) {
						equal = (na == 0 && nb == 0);
						break;
					}
					if (na == 0) {
						break;
					}
				}
				c_changed = !equal;
			}
		}
	}

	if (c_changed) {
		if (rename(tmp_c_path, c_path) != 0) {
			return Err("failed to update manifest '%s'", c_path);
		}
	} else {
		remove(tmp_c_path);
	}
	f64 pre_ms = time_ms() - t0;

	bool needs_compile = c_changed || !fs_file_exists(so_path);
	if (!needs_compile) {
		struct stat st_c, st_so;
		if (stat(so_path, &st_so) == 0 && stat(c_path, &st_c) == 0 &&
			st_so.st_mtime >= st_c.st_mtime) {
			needs_compile = false;
		} else {
			needs_compile = true;
		}
	}

	t0 = time_ms();
	if (needs_compile) {
		Try(compile_manifest(c_path, so_path));
	}
	f64 comp_ms = time_ms() - t0;

	Project *proj = Unwrap(load_manifest(so_path));
	defer(unload_manifest);

	Target *matched_target = NULL;
	if (target_name && target_name[0]) {
		if (proj->targets) {
			for (void **t = proj->targets; *t; ++t) {
				Target *tgt = (Target *)*t;
				if (strcmp(tgt->name, target_name) == 0) {
					matched_target = tgt;
					break;
				}
			}
		}
		if (!matched_target) {
			return Err(ERR_TARGET_NOT_FOUND, target_name, proj->name);
		}
		if (out_bin && matched_target->kind != T_EXECUTABLE) {
			return Err(ERR_NOT_EXECUTABLE, target_name);
		}
	}

	printf(
		"\n  %s%sBuilding%s %s%s%s v%s [%s]\n", th.bold, th.teal, th.reset, th.white, proj->name,
		th.reset, proj->version ? proj->version : NOUR_VERSION, profile ? profile : "debug"
	);

	ui_tree_step(false, "preprocess manifest", pre_ms);
	ui_tree_step(false, "load manifest [libnour.so]", comp_ms);

	t0 = time_ms();
	Result res = build_target(proj, project_dir, target_name, profile);
	f64 build_ms = time_ms() - t0;

	if (!res.ok) {
		return res;
	}

	ui_tree_step(true, "compile targets", build_ms);

	f64 total_duration = time_ms() - total_start;
	char dur_str[32];
	ui_format_duration(dur_str, sizeof(dur_str), total_duration);
	printf("\n");
	ui_success("Finished build in %s", dur_str);

	// Record clean manifest in .nour directory
	const char *clean_file = rnstr_sprintf(&a, "%s/.nour/clean.txt", project_dir);
	FILE *cf = fopen(clean_file, "w");
	if (cf) {
		defer(fclose, cf);
		fprintf(cf, "%s\n.nour\n", project_build_dir(proj));
	}

	if (out_bin && out_bin_cap > 0) {
		out_bin[0] = '\0';
		char out_dir_buf[128];
		project_out_dir(out_dir_buf, sizeof(out_dir_buf), proj, profile);
		const char *bdir = out_dir_buf;
		const char *resolved_target = NULL;

		if (matched_target) {
			resolved_target = matched_target->name;
		} else if (proj->targets) {
			usize exec_count = 0;
			for (void **t = proj->targets; *t; ++t) {
				Target *tgt = (Target *)*t;
				if (tgt->kind == T_EXECUTABLE) {
					exec_count++;
					resolved_target = tgt->name;
				}
			}
			if (exec_count > 1) {
				return Err(ERR_AMBIGUOUS_EXEC, proj->name);
			}
		}

		if (resolved_target) {
			const char *bin_path = rnstr_path(&a, project_dir, bdir, resolved_target, NULL);
			strncpy(out_bin, bin_path, out_bin_cap - 1);
			out_bin[out_bin_cap - 1] = '\0';
		}
	}

	return Ok(NULL);
}

Result cmd_build(Context *ctx)
{
	const char *project_dir = flag(ctx, "dir", ".");
	const char *target_name = flag(ctx, "target", "");
	const char *profile = resolve_profile(ctx);
	if (ctx->n_args > 0)
		project_dir = ctx->args[0];

	return execute_project_build(
		project_dir, (target_name && target_name[0]) ? target_name : NULL, profile, NULL, 0
	);
}

Result cmd_run(Context *ctx)
{
	const char *project_dir = flag(ctx, "dir", ".");
	const char *target_name = flag(ctx, "target", "");
	const char *profile = resolve_profile(ctx);

	if (strcmp(project_dir, ".") == 0 && ctx->n_args > 0) {
		project_dir = ctx->args[0];
	}

	char bin_path[PATH_MAX] = {0};
	Try(execute_project_build(project_dir, target_name, profile, bin_path, sizeof(bin_path)));

	if (!bin_path[0]) {
		return Err(ERR_NO_EXE_TARGET, project_dir);
	}

	arena_scratch(a, 2048);
	strbuf_open(&a);
	strbuf_printf(&a, "\"%s\"", bin_path);

	for (u32 i = 0; i < ctx->n_forwarded; ++i) {
		const char *arg = ctx->forwarded_args[i];
		strbuf_append(&a, " \"");
		for (const char *p = arg; *p; ++p) {
			if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
				strbuf_putc(&a, '\\');
			}
			strbuf_putc(&a, *p);
		}
		strbuf_putc(&a, '"');
	}

	const char *cmd_line = strbuf_finish(&a);
	ui_running(cmd_line);
	i32 ret = system(cmd_line);

	if (ret != 0) {
		i32 code = WIFEXITED(ret) ? WEXITSTATUS(ret) : WIFSIGNALED(ret) ? 128 + WTERMSIG(ret) : ret;
		return Err(ERR_TARGET_EXIT, bin_path, code);
	}

	return Ok(NULL);
}

// Refuse paths that could escape the project directory during clean.
static bool is_safe_rel_path(const char *rel)
{
	if (!rel || !rel[0] || rel[0] == '/')
		return false;
	if (strcmp(rel, ".") == 0)
		return false;

	const char *p = rel;
	while (*p) {
		const char *e = strchr(p, '/');
		usize clen = e ? (usize)(e - p) : strlen(p);
		if (clen == 0 && e)
			return false; // '//' — suspicious
		if (clen == 2 && p[0] == '.' && p[1] == '.')
			return false; // parent traversal
		p = e ? e + 1 : p + clen;
	}
	return true;
}

Result cmd_clean(Context *ctx)
{
	const char *project_dir = flag(ctx, "dir", ".");
	if (ctx->n_args > 0)
		project_dir = ctx->args[0];

	arena_scratch(a, 4096);

	const char *clean_manifest = rnstr_sprintf(&a, "%s/.nour/clean.txt", project_dir);
	FILE *f = fopen(clean_manifest, "r");
	if (f) {
		defer(fclose, f);
		char entry[PATH_MAX];
		while (fgets(entry, sizeof(entry), f)) {
			StrView trimmed = sv_trim(sv_fromc(entry));
			if (sv_empty(trimmed))
				continue;

			char *rel = rnstr_sprintf(&a, "%.*s", (int)trimmed.len, trimmed.data);
			if (!is_safe_rel_path(rel)) {
				return Err("refusing to remove unsafe path '%s' listed in clean.txt", rel);
			}

			const char *tpath =
				rnstr_sprintf(&a, "%s/%.*s", project_dir, (int)trimmed.len, trimmed.data);

			if (fs_dir_exists(tpath)) {
				Try(fs_remove_dir(tpath));
				ui_status("Removed", "%s/", tpath);
			} else if (fs_file_exists(tpath)) {
				remove(tpath);
				ui_status("Removed", "%s", tpath);
			}
		}
	}

	const char *dot_nour_dir = rnstr_sprintf(&a, "%s/.nour", project_dir);
	if (fs_dir_exists(dot_nour_dir)) {
		Try(fs_remove_dir(dot_nour_dir));
		ui_status("Removed", "%s/", dot_nour_dir);
	}

	const char *default_build_dir = rnstr_sprintf(&a, "%s/build", project_dir);
	if (fs_dir_exists(default_build_dir)) {
		Try(fs_remove_dir(default_build_dir));
		ui_status("Removed", "%s/", default_build_dir);
	}

	return Ok(NULL);
}
