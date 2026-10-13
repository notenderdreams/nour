#include "builder.h"
#include "arena.h"
#include "fs.h"
#include "templates.h"
#include "utils.h"

#include <glob.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Result generate_config_header(
	const Project *proj, const Target *target, const char *project_dir, const char *profile
) {
	arena_scratch(a, 4096);

	const char *dot_nour = rnstr_sprintf(&a, "%s/.nour", project_dir);
	Result r = fs_create_dir(dot_nour);
	if (!r.ok)
		return r;

	const char *dot_nour_inc = rnstr_sprintf(&a, "%s/.nour/nour", project_dir);
	r = fs_create_dir(dot_nour_inc);
	if (!r.ok)
		return r;

	const char *header_path = rnstr_sprintf(&a, "%s/.nour/nour/config.h", project_dir);

	const char *proj_name = (proj->name && proj->name[0]) ? proj->name : "app";
	const char *proj_ver = (proj->version && proj->version[0]) ? proj->version : "0.1.0";
	const char *build_dir = project_build_dir(proj);
	const char *tgt_name = (target && target->name) ? target->name : proj_name;
	const char *tgt_kind = (target && target->kind == T_LIBRARY) ? "library" : "executable";

	// Resolve profile and sanitize to uppercase macro
	const char *prof_str = (profile && profile[0]) ? profile : "debug";
	char prof_upper[64];
	sanitize_macro_name(prof_str, prof_upper, sizeof(prof_upper));

	// Mode tier resolution
	bool is_debug = (strcmp(prof_str, "debug") == 0);
	bool is_release = (strcmp(prof_str, "release") == 0);
	i32 nour_debug = is_debug ? 1 : 0;
	i32 nour_release = is_release ? 1 : 0;

	i32 major = 0, minor = 0, patch = 0;
	parse_semver(proj_ver, &major, &minor, &patch);

	// Format template
	const char *content = rnstr_sprintf(
		&a, CONFIG_HEADER_TEMPLATE, proj_name, proj_ver, build_dir, major, minor, patch, tgt_name,
		tgt_kind, prof_str, prof_upper, nour_debug, nour_release
	);

	FILE *existing = fopen(header_path, "r");
	if (existing) {
		defer(fclose, existing);
		char existing_buf[2048];
		usize n = fread(existing_buf, 1, sizeof(existing_buf) - 1, existing);
		existing_buf[n] = '\0';
		if (n == strlen(content) && strcmp(existing_buf, content) == 0) {
			return Ok(NULL);
		}
	}

	return fs_create_file(header_path, content);
}

static const char *resolve_executable_root(const Executable *exe, const char *project_dir)
{
	arena_scratch(a, 512);

	if (exe->root && exe->root[0]) {
		const char *path = rnstr_sprintf(&a, "%s/%s", project_dir, exe->root);
		if (!fs_file_exists(path))
			return NULL;
		return exe->root;
	}

	const char *src_main = rnstr_sprintf(&a, "%s/src/main.c", project_dir);
	if (fs_file_exists(src_main))
		return "src/main.c";

	const char *main_c = rnstr_sprintf(&a, "%s/main.c", project_dir);
	if (fs_file_exists(main_c))
		return "main.c";

	return NULL;
}

static void
collect_module_includes(Arena *a, const Module *mod, const char *project_dir, usize depth)
{
	if (!mod || depth > 16)
		return;

	if (mod->includes) {
		for (const char **inc = mod->includes; *inc; ++inc) {
			rnstr_printf(a, " -I\"%s/%s\"", project_dir, *inc);
		}
	}

	if (mod->mods) {
		for (usize i = 0; mod->mods[i]; ++i) {
			collect_module_includes(a, mod->mods[i], project_dir, depth + 1);
		}
	}
}

static Result collect_module_sources(
	Arena *a, const Module *mod, const char *project_dir, const char *full_root_path,
	bool *root_appended, usize *total_sources, usize depth
) {
	if (!mod || depth > 16)
		return Ok(NULL);

	if (mod->sources) {
		for (const char **src = mod->sources; *src; ++src) {
			char pattern[PATH_MAX];
			snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

			glob_t g;
			i32 ret = glob(pattern, 0, NULL, &g);
			if (ret) {
				const char *mod_name = mod->name ? mod->name : "anonymous";
				return Err(ERR_MODULE_NO_MATCH, mod_name, *src);
			}
			defer(globfree, &g);

			for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
				if (full_root_path && strcmp(g.gl_pathv[k], full_root_path) == 0) {
					*root_appended = true;
				}
				rnstr_printf(a, " \"%s\"", g.gl_pathv[k]);
				(*total_sources)++;
			}
		}
	}

	if (mod->mods) {
		for (usize i = 0; mod->mods[i]; ++i) {
			Try(collect_module_sources(
				a, mod->mods[i], project_dir, full_root_path, root_appended, total_sources,
				depth + 1
			));
		}
	}

	return Ok(NULL);
}

static Result build_executable(
	const Executable *exe, const Project *proj, const char *project_dir, const char *profile
) {
	const char *cc = (proj->cc && proj->cc[0]) ? proj->cc : "cc";
	const char *build_dir = project_build_dir(proj);

	Try(generate_config_header(proj, (const Target *)exe, project_dir, profile));

	const char *root = resolve_executable_root(exe, project_dir);
	if (exe->root && !root) {
		return Err(ERR_TARGET_NO_ROOT, exe->name, exe->root);
	}

	arena_scratch(a, 16 * 1024);

	const char *full_root_path = root ? rnstr_sprintf(&a, "%s/%s", project_dir, root) : NULL;
	bool root_appended = false;

	const char *out_path = rnstr_path(&a, project_dir, build_dir, exe->name, NULL);

	rnstr_begin(&a);
	rnstr_printf(&a, "%s -o \"%s\"", cc, out_path);

	// Cflags
	if (exe->cflags) {
		for (const char **f = exe->cflags; *f; ++f) {
			rnstr_printf(&a, " %s", *f);
		}
	}

	// Generated header include path (.nour)
	rnstr_printf(&a, " -I\"%s/.nour\"", project_dir);

	// Includes
	if (exe->includes) {
		for (const char **inc = exe->includes; *inc; ++inc) {
			rnstr_printf(&a, " -I\"%s/%s\"", project_dir, *inc);
		}
	}

	// Module includes
	if (exe->mods) {
		for (usize i = 0; exe->mods[i]; ++i) {
			collect_module_includes(&a, exe->mods[i], project_dir, 0);
		}
	}

	// Defines
	if (exe->defines) {
		for (const char **d = exe->defines; *d; ++d) {
			rnstr_printf(&a, " -D%s", *d);
		}
	}

	// Source files
	if ((!exe->sources || !exe->sources[0]) && !root && (!exe->mods || !exe->mods[0])) {
		return Err(ERR_TARGET_NO_SOURCES, exe->name);
	}

	usize total_sources = 0;
	if (exe->sources) {
		for (const char **src = exe->sources; *src; ++src) {
			char pattern[PATH_MAX];
			snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

			glob_t g;
			i32 ret = glob(pattern, 0, NULL, &g);
			if (ret) {
				return Err(ERR_TARGET_NO_MATCH, exe->name, *src);
			}
			defer(globfree, &g);

			for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
				if (root && strcmp(g.gl_pathv[k], full_root_path) == 0) {
					root_appended = true;
				}
				rnstr_printf(&a, " \"%s\"", g.gl_pathv[k]);
				++total_sources;
			}
		}
	}

	// Module sources
	if (exe->mods) {
		for (usize i = 0; exe->mods[i]; ++i) {
			Try(collect_module_sources(
				&a, exe->mods[i], project_dir, full_root_path, &root_appended, &total_sources, 0
			));
		}
	}

	// If root wasn't already in sources glob, append it
	if (root && !root_appended) {
		rnstr_printf(&a, " \"%s\"", full_root_path);
		++total_sources;
	}

	if (!total_sources) {
		return Err(ERR_TARGET_ZERO_SOURCES, exe->name);
	}

	// Link flags
	if (exe->ldflags) {
		for (const char **ld = exe->ldflags; *ld; ++ld) {
			rnstr_printf(&a, " %s", *ld);
		}
	}

	// Execute build
	const char *cmd = rnstr_finish(&a);
	i32 status = system(cmd);
	if (status) {
		return Err(ERR_BUILD_FAILED, exe->name);
	}

	return Ok(NULL);
}

Result build_project_target(
	const Project *proj, const char *project_dir, const char *target_name, const char *profile
) {
	if (!proj->targets) {
		return Ok(NULL);
	}

	const char *build_dir = project_build_dir(proj);
	arena_scratch(a, 256);
	const char *build_path = rnstr_sprintf(&a, "%s/%s", project_dir, build_dir);
	Result r = fs_create_dir(build_path);
	Try(r);

	if (target_name && target_name[0]) {
		for (usize i = 0; proj->targets[i]; ++i) {
			Target *target = (Target *)proj->targets[i];
			if (strcmp(target->name, target_name) == 0) {
				if (target->kind == T_EXECUTABLE) {
					return build_executable((Executable *)target, proj, project_dir, profile);
				} else if (target->kind == T_LIBRARY) {
					return Err(ERR_LIB_UNIMPLEMENTED, target->name);
				} else {
					return Err(ERR_UNKNOWN_TARGET, target->name);
				}
			}
		}
		return Err(ERR_TARGET_NOT_FOUND, target_name, proj->name);
	}

	for (usize i = 0; proj->targets[i]; ++i) {
		Target *target = (Target *)proj->targets[i];

		if (target->kind == T_EXECUTABLE) {
			Try(build_executable((Executable *)target, proj, project_dir, profile));
		} else if (target->kind == T_LIBRARY) {
			return Err(ERR_LIB_UNIMPLEMENTED, target->name);
		} else {
			return Err(ERR_UNKNOWN_TARGET, target->name);
		}
	}

	return Ok(NULL);
}

Result build_project(const Project *proj, const char *project_dir)
{
	return build_project_target(proj, project_dir, NULL, "debug");
}
