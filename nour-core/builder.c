#include "builder.h"
#include "fs.h"
#include "utils.h"
#include "templates.h"

#include <glob.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Result generate_config_header(
	const Project *proj, const Target *target, const char *project_dir, const char *profile
)
{
	char dot_nour[PATH_MAX];
	snprintf(dot_nour, sizeof(dot_nour), "%s/.nour", project_dir);
	Try(fs_create_dir(dot_nour));

	char dot_nour_inc[PATH_MAX];
	snprintf(dot_nour_inc, sizeof(dot_nour_inc), "%s/.nour/nour", project_dir);
	Try(fs_create_dir(dot_nour_inc));

	char header_path[PATH_MAX];
	snprintf(header_path, sizeof(header_path), "%s/.nour/nour/config.h", project_dir);

	const char *proj_name = (proj->name && proj->name[0]) ? proj->name : "app";
	const char *proj_ver  = (proj->version && proj->version[0]) ? proj->version : "0.1.0";
	const char *build_dir = project_build_dir(proj);
	const char *tgt_name  = (target && target->name) ? target->name : proj_name;
	const char *tgt_kind  = (target && target->kind == T_LIBRARY) ? "library" : "executable";

	// Resolve profile and sanitize to uppercase macro
	const char *prof_str = (profile && profile[0]) ? profile : "debug";
	char		prof_upper[64];
	sanitize_macro_name(prof_str, prof_upper, sizeof(prof_upper));

	// Mode tier resolution
	bool is_debug	  = (strcmp(prof_str, "debug") == 0);
	bool is_release	  = (strcmp(prof_str, "release") == 0);
	i32	 nour_debug	  = is_debug ? 1 : 0;
	i32	 nour_release = is_release ? 1 : 0;

	i32 major = 0, minor = 0, patch = 0;
	parse_semver(proj_ver, &major, &minor, &patch);

	// Format template from nour-core/templates.h
	char content[2048];
	i32	 len = snprintf(
		 content, sizeof(content), CONFIG_HEADER_TEMPLATE, proj_name, proj_ver, build_dir, major,
		 minor, patch, tgt_name, tgt_kind, prof_str, prof_upper, nour_debug, nour_release
	 );

	if (len < 0 || (usize)len >= sizeof(content)) {
		return Err("Configuration header content exceeded buffer size");
	}

	FILE *existing = fopen(header_path, "r");
	if (existing) {
		char  existing_buf[2048];
		usize n = fread(existing_buf, 1, sizeof(existing_buf) - 1, existing);
		fclose(existing);
		existing_buf[n] = '\0';
		if (n == (usize)len && strcmp(existing_buf, content) == 0) {
			return Ok(NULL);
		}
	}

	Try(fs_create_file(header_path, content));

	return Ok(NULL);
}

__attribute__((format(printf, 4, 5))) static bool
cmd_append(char *buf, usize cap, usize *off, const char *fmt, ...)
{
	if (*off >= cap)
		return false;

	va_list ap;
	va_start(ap, fmt);
	i32 n = vsnprintf(buf + *off, cap - *off, fmt, ap);
	va_end(ap);

	if (n < 0 || (usize)n >= cap - *off) {
		*off = cap;
		return false;
	}
	*off += (usize)n;
	return true;
}

static const char *resolve_executable_root(const Executable *exe, const char *project_dir)
{
	if (exe->root && exe->root[0]) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", project_dir, exe->root);
		if (!fs_file_exists(path)) {
			return NULL;
		}
		return exe->root;
	}

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/src/main.c", project_dir);
	if (fs_file_exists(path)) {
		return "src/main.c";
	}

	snprintf(path, sizeof(path), "%s/main.c", project_dir);
	if (fs_file_exists(path)) {
		return "main.c";
	}

	return NULL;
}

static Result build_executable(
	const Executable *exe, const Project *proj, const char *project_dir, const char *profile
)
{
	const char *cc		  = (proj->cc && proj->cc[0]) ? proj->cc : "cc";
	const char *build_dir = project_build_dir(proj);

	Try(generate_config_header(proj, (const Target *)exe, project_dir, profile));

	const char *root = resolve_executable_root(exe, project_dir);
	if (exe->root && !root) {
		return Err("Target '%s': root file '%s' not found", exe->name, exe->root);
	}

	char full_root_path[PATH_MAX] = { 0 };
	if (root) {
		snprintf(full_root_path, sizeof(full_root_path), "%s/%s", project_dir, root);
	}
	bool root_appended = false;

	char out_path[PATH_MAX];
	snprintf(out_path, sizeof(out_path), "%s/%s/%s", project_dir, build_dir, exe->name);

	char  cmd[4096];
	usize offset = 0;

	if (!cmd_append(cmd, sizeof(cmd), &offset, "%s -o \"%s\"", cc, out_path)) {
		return Err("Target '%s': command line too long", exe->name);
	}

	// Cflags
	if (exe->cflags) {
		for (const char **f = exe->cflags; *f; ++f) {
			if (!cmd_append(cmd, sizeof(cmd), &offset, " %s", *f))
				return Err("Target '%s': command line too long", exe->name);
		}
	}

	// Generated header include path (.nour)
	if (!cmd_append(cmd, sizeof(cmd), &offset, " -I\"%s/.nour\"", project_dir)) {
		return Err("Target '%s': command line too long", exe->name);
	}

	// Includes
	if (exe->includes) {
		for (const char **inc = exe->includes; *inc; ++inc) {
			if (!cmd_append(cmd, sizeof(cmd), &offset, " -I\"%s/%s\"", project_dir, *inc))
				return Err("Target '%s': command line too long", exe->name);
		}
	}

	// Defines
	if (exe->defines) {
		for (const char **d = exe->defines; *d; ++d) {
			if (!cmd_append(cmd, sizeof(cmd), &offset, " -D%s", *d))
				return Err("Target '%s': command line too long", exe->name);
		}
	}

	// Source files
	if ((!exe->sources || !exe->sources[0]) && !root) {
		return Err("Target '%s' has no sources specified", exe->name);
	}

	usize total_sources = 0;
	if (exe->sources) {
		for (const char **src = exe->sources; *src; ++src) {
			char pattern[PATH_MAX];
			snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

			glob_t g;
			i32	   ret = glob(pattern, 0, NULL, &g);
			if (ret) {
				return Err("Target '%s': no files found matching '%s'", exe->name, *src);
			}

			for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
				if (root && strcmp(g.gl_pathv[k], full_root_path) == 0) {
					root_appended = true;
				}
				if (!cmd_append(cmd, sizeof(cmd), &offset, " \"%s\"", g.gl_pathv[k])) {
					globfree(&g);
					return Err("Target '%s': command line too long", exe->name);
				}
				++total_sources;
			}
			globfree(&g);
		}
	}

	// If root wasn't already in sources glob, append it
	if (root && !root_appended) {
		if (!cmd_append(cmd, sizeof(cmd), &offset, " \"%s\"", full_root_path)) {
			return Err("Target '%s': command line too long", exe->name);
		}
		++total_sources;
	}

	if (!total_sources) {
		return Err("Target '%s': zero source files resolved", exe->name);
	}

	// Link flags
	if (exe->ldflags) {
		for (const char **ld = exe->ldflags; *ld; ++ld) {
			if (!cmd_append(cmd, sizeof(cmd), &offset, " %s", *ld))
				return Err("Target '%s': command line too long", exe->name);
		}
	}

	// Execute build
	i32 status = system(cmd);
	if (status) {
		return Err("Build failed for target '%s'", exe->name);
	}

	return Ok(NULL);
}

Result build_project_target(
	const Project *proj, const char *project_dir, const char *target_name, const char *profile
)
{
	if (!proj->targets) {
		return Ok(NULL);
	}

	const char *build_dir = project_build_dir(proj);
	char		build_path[PATH_MAX];
	snprintf(build_path, sizeof(build_path), "%s/%s", project_dir, build_dir);
	Try(fs_create_dir(build_path));

	if (target_name && target_name[0]) {
		for (usize i = 0; proj->targets[i]; ++i) {
			Target *target = (Target *)proj->targets[i];
			if (strcmp(target->name, target_name) == 0) {
				if (target->kind == T_EXECUTABLE) {
					return build_executable((Executable *)target, proj, project_dir, profile);
				} else if (target->kind == T_LIBRARY) {
					return Err("Target '%s': library builds are not yet implemented", target->name);
				} else {
					return Err("Target '%s': unknown target kind", target->name);
				}
			}
		}
		return Err("Target '%s' not found in project '%s'", target_name, proj->name);
	}

	for (usize i = 0; proj->targets[i]; ++i) {
		Target *target = (Target *)proj->targets[i];

		if (target->kind == T_EXECUTABLE) {
			Try(build_executable((Executable *)target, proj, project_dir, profile));
		} else if (target->kind == T_LIBRARY) {
			return Err("Target '%s': library builds are not yet implemented", target->name);
		} else {
			return Err("Target '%s': unknown target kind", target->name);
		}
	}

	return Ok(NULL);
}

Result build_project(const Project *proj, const char *project_dir)
{
	return build_project_target(proj, project_dir, NULL, "debug");
}
