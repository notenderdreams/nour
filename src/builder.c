#include "builder.h"
#include "utils.h"
#include <glob.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((format(printf, 4, 5)))
static bool cmd_append(char *buf, usize cap, usize *off, const char *fmt, ...)
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

static Result build_executable(const Executable *exe, const Project *proj, const char *project_dir)
{
	const char *cc		  = (proj->cc && proj->cc[0]) ? proj->cc : "cc";
	const char *build_dir = project_build_dir(proj);

	char out_path[512];
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
	if (!exe->sources || !exe->sources[0]) {
		return Err("Target '%s' has no sources specified", exe->name);
	}

	usize total_sources = 0;
	for (const char **src = exe->sources; *src; ++src) {
		char pattern[512];
		snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

		glob_t g;
		i32	   ret = glob(pattern, 0, NULL, &g);
		if (ret) {
			return Err("Target '%s': no files found matching '%s'", exe->name, *src);
		}

		for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
			if (!cmd_append(cmd, sizeof(cmd), &offset, " \"%s\"", g.gl_pathv[k])) {
				globfree(&g);
				return Err("Target '%s': command line too long", exe->name);
			}
			++total_sources;
		}
		globfree(&g);
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

Result build_project(const Project *proj, const char *project_dir)
{
	if (!proj->targets) {
		return Ok(NULL);
	}

	const char *build_dir = project_build_dir(proj);
	char		mkdir_cmd[1024];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s/%s\"", project_dir, build_dir);
	i32 ret = system(mkdir_cmd);
	if (ret) {
		return Err("Failed to create build directory '%s/%s'", project_dir, build_dir);
	}

	for (usize i = 0; proj->targets[i]; ++i) {
		Target *target = (Target *)proj->targets[i];

		if (target->kind == T_EXECUTABLE) {
			TRY(build_executable((Executable *)target, proj, project_dir));
		} else if (target->kind == T_LIBRARY) {
			return Err("Target '%s': library builds are not yet implemented", target->name);
		} else {
			return Err("Target '%s': unknown target kind", target->name);
		}
	}

	return Ok(NULL);
}
