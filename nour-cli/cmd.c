#include "cmd.h"
#include "builder.h"
#include "fs.h"
#include "loader.h"
#include "parser.h"
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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUILD_FLAGS                                                                                \
	{                                                                                              \
		{ "dir", 'C', FLAG_STR, .val.s = ".", "project directory" },                               \
		{ "target", 't', FLAG_STR, .val.s = "", "specific build target" },                         \
		{ "profile", 'p', FLAG_STR, .val.s = "", "build profile (e.g. debug, release, staging)" }, \
		{ "release", 'r', FLAG_BOOL, .val.b = false, "shorthand for --profile release" },          \
		FLAG_END,                                                                                  \
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
		{ "dir", 'C', FLAG_STR, .val.s = ".", "project directory" },
		FLAG_END,
	},
};

static Result scaffold_project(const char *name, const char *base_dir)
{
	char src_dir[PATH_MAX], nour_file[PATH_MAX], main_file[PATH_MAX], gitignore_file[PATH_MAX];
	snprintf(src_dir, sizeof(src_dir), "%s/src", base_dir);
	snprintf(nour_file, sizeof(nour_file), "%s/project.nour", base_dir);
	snprintf(main_file, sizeof(main_file), "%s/src/main.c", base_dir);
	snprintf(gitignore_file, sizeof(gitignore_file), "%s/.gitignore", base_dir);

	Try(fs_create_dir(src_dir));
	ui_created(src_dir);

	char buf[2048];
	snprintf(buf, sizeof(buf), MANIFEST_TEMPLATE, name, name, name);
	Try(fs_create_file(nour_file, buf));
	ui_created(nour_file);

	snprintf(buf, sizeof(buf), MAIN_TEMPLATE, name);
	Try(fs_create_file(main_file, buf));
	ui_created(main_file);

	Try(fs_create_file(gitignore_file, GITIGNORE_TEMPLATE));
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
		return Err("invalid project name '%s'. Must be a valid C identifier", project_name);
	}

	if (fs_dir_exists(project_name)) {
		return Err("directory '%s' already exists", project_name);
	}

	Try(fs_create_dir(project_name));
	ui_created(project_name);

	Try(scaffold_project(project_name, project_name));
	ui_success("Project '%s' created successfully!", project_name);
	return Ok(NULL);
}

Result cmd_init(Context *ctx)
{
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd)))
		return Err("failed to get current working directory");

	char *project_name = strrchr(cwd, '/');
	project_name	   = project_name ? project_name + 1 : cwd;

	if (ctx->n_args > 0) {
		project_name = (char *)ctx->args[0];
	}

	if (!is_valid_name(project_name)) {
		return Err("invalid project name '%s'. Must be a valid C identifier", project_name);
	}

	if (fs_file_exists("project.nour")) {
		return Err("'project.nour' already exists in current directory");
	}

	Try(scaffold_project(project_name, "."));
	ui_success("Initialized Nour project in current directory.");
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
)
{
	char nour_path[PATH_MAX], c_path[PATH_MAX], so_path[PATH_MAX], dot_nour_dir[PATH_MAX];
	snprintf(nour_path, sizeof(nour_path), "%s/project.nour", project_dir);
	snprintf(dot_nour_dir, sizeof(dot_nour_dir), "%s/.nour", project_dir);
	snprintf(c_path, sizeof(c_path), "%s/.nour/project.nour.c", project_dir);
	snprintf(so_path, sizeof(so_path), "%s/.nour/libnour.so", project_dir);

	if (!fs_file_exists(nour_path)) {
		return Err("could not find '%s'", nour_path);
	}

	f64 total_start = time_ms();
	Try(fs_create_dir(dot_nour_dir));

	f64 t0 = time_ms();
	Try(preprocess(nour_path, c_path));
	f64 pre_ms = time_ms() - t0;

	t0 = time_ms();
	Try(compile_nour(c_path, so_path));
	f64 comp_ms = time_ms() - t0;

	Project *proj = Unwrap(load_project(so_path));

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
			Result r = Err("target '%s' not found in project '%s'", target_name, proj->name);
			loader_close();
			return r;
		}
		if (out_bin && matched_target->kind != T_EXECUTABLE) {
			Result r = Err("target '%s' is not an executable", target_name);
			loader_close();
			return r;
		}
	}

	printf(
		"\n  %s%sBuilding%s %s%s%s v%s [%s]\n", th.bold, th.teal, th.reset, th.white, proj->name,
		th.reset, proj->version ? proj->version : NOUR_VERSION, profile ? profile : "debug"
	);

	ui_tree_step(false, "preprocess manifest", pre_ms);
	ui_tree_step(false, "load manifest [libnour.so]", comp_ms);

	t0				= time_ms();
	Result res		= build_project_target(proj, project_dir, target_name, profile);
	f64	   build_ms = time_ms() - t0;

	if (!res.ok) {
		loader_close();
		return res;
	}

	ui_tree_step(true, "compile targets", build_ms);

	f64 total_duration = time_ms() - total_start;
	ui_success("Finished build in %.2f ms", total_duration);

	// Record clean manifest in .nour directory
	char clean_file[PATH_MAX];
	snprintf(clean_file, sizeof(clean_file), "%s/.nour/clean.txt", project_dir);
	FILE *cf = fopen(clean_file, "w");
	if (cf) {
		fprintf(cf, "%s\n.nour\n", project_build_dir(proj));
		fclose(cf);
	}

	if (out_bin && out_bin_cap > 0) {
		out_bin[0]					= '\0';
		const char *bdir			= project_build_dir(proj);
		const char *resolved_target = NULL;

		if (matched_target) {
			resolved_target = matched_target->name;
		} else if (proj->targets) {
			for (void **t = proj->targets; *t; ++t) {
				Target *tgt = (Target *)*t;
				if (tgt->kind == T_EXECUTABLE) {
					resolved_target = tgt->name;
					break;
				}
			}
		}

		if (resolved_target) {
			snprintf(out_bin, out_bin_cap, "%s/%s/%s", project_dir, bdir, resolved_target);
		}
	}

	loader_close();
	return Ok(NULL);
}

Result cmd_build(Context *ctx)
{
	const char *project_dir = flag(ctx, "dir", ".");
	const char *target_name = flag(ctx, "target", "");
	const char *profile		= resolve_profile(ctx);
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
	const char *profile		= resolve_profile(ctx);

	if (strcmp(project_dir, ".") == 0 && ctx->n_args > 0) {
		project_dir = ctx->args[0];
	}

	char bin_path[PATH_MAX] = { 0 };
	Try(execute_project_build(project_dir, target_name, profile, bin_path, sizeof(bin_path)));

	if (!bin_path[0]) {
		return Err("no executable target found to run in project '%s'", project_dir);
	}

	char run_cmd[4096];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);

	for (u32 i = 0; i < ctx->n_forwarded; ++i) {
		const char *arg = ctx->forwarded_args[i];
		usize		rem = sizeof(run_cmd) - strlen(run_cmd) - 1;
		if (rem < strlen(arg) + 4)
			break;
		strncat(run_cmd, " \"", rem);
		rem = sizeof(run_cmd) - strlen(run_cmd) - 1;
		for (const char *p = arg; *p && rem > 2; ++p) {
			if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
				strncat(run_cmd, "\\", rem);
				rem--;
			}
			char ch[2] = { *p, '\0' };
			strncat(run_cmd, ch, rem);
			rem--;
		}
		strncat(run_cmd, "\"", rem);
	}

	ui_running(run_cmd);
	i32 ret = system(run_cmd);
	if (ret != 0) {
		i32 code = WIFEXITED(ret) ? WEXITSTATUS(ret) : WIFSIGNALED(ret) ? 128 + WTERMSIG(ret) : ret;
		return Err("target '%s' exited with code %d", bin_path, code);
	}

	return Ok(NULL);
}

Result cmd_clean(Context *ctx)
{
	const char *project_dir = flag(ctx, "dir", ".");
	if (ctx->n_args > 0)
		project_dir = ctx->args[0];

	char clean_manifest[PATH_MAX];
	snprintf(clean_manifest, sizeof(clean_manifest), "%s/.nour/clean.txt", project_dir);

	FILE *f = fopen(clean_manifest, "r");
	if (f) {
		char entry[PATH_MAX];
		while (fgets(entry, sizeof(entry), f)) {
			char *nl = strchr(entry, '\n');
			if (nl)
				*nl = '\0';
			char *trimmed = entry;
			while (*trimmed && isspace((unsigned char)*trimmed))
				trimmed++;
			if (!*trimmed)
				continue;

			char target_path[PATH_MAX];
			snprintf(target_path, sizeof(target_path), "%s/%s", project_dir, trimmed);
			if (fs_dir_exists(target_path)) {
				Try(fs_remove_dir(target_path));
				ui_status("Removed", "%s/", target_path);
			} else if (fs_file_exists(target_path)) {
				remove(target_path);
				ui_status("Removed", "%s", target_path);
			}
		}
		fclose(f);
	}

	char dot_nour_dir[PATH_MAX];
	snprintf(dot_nour_dir, sizeof(dot_nour_dir), "%s/.nour", project_dir);
	if (fs_dir_exists(dot_nour_dir)) {
		Try(fs_remove_dir(dot_nour_dir));
		ui_status("Removed", "%s/", dot_nour_dir);
	}

	char default_build_dir[PATH_MAX];
	snprintf(default_build_dir, sizeof(default_build_dir), "%s/build", project_dir);
	if (fs_dir_exists(default_build_dir)) {
		Try(fs_remove_dir(default_build_dir));
		ui_status("Removed", "%s/", default_build_dir);
	}

	return Ok(NULL);
}
