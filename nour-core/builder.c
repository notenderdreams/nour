#include "builder.h"
#include "arena.h"
#include "deps.h"
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
	char od_buf[96];
	project_out_dir(od_buf, sizeof(od_buf), proj, profile);
	const char *build_dir = od_buf;
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

	return fs_write_file(header_path, content);
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
			strbuf_printf(a, " -I%s", sh_quote(a, rnstr_path(a, project_dir, *inc, NULL)));
		}
	}

	if (mod->mods) {
		for (usize i = 0; mod->mods[i]; ++i) {
			collect_module_includes(a, mod->mods[i], project_dir, depth + 1);
		}
	}
}

// Optional collector so callers can get module source paths instead of having
// them printed straight into the command string (needed for per-object
// static library compilation).
typedef struct {
	const char **items;
	usize		 count;
	usize		 cap;
} StrList;

static void strlist_free(StrList *l)
{
	free(l->items);
	l->items = NULL;
	l->count = 0;
	l->cap = 0;
}

static bool strlist_push(Arena *a, StrList *l, const char *s)
{
	if (l->count == l->cap) {
		usize ncap = l->cap ? l->cap * 2 : 32;
		const char **ni = realloc(l->items, ncap * sizeof(const char *));
		if (!ni)
			return false;
		l->items = ni;
		l->cap = ncap;
	}
	l->items[l->count++] = rnstr_dup(a, s);
	return true;
}

static Result collect_module_sources(
	Arena *a, const Module *mod, const char *project_dir, const char *full_root_path,
	bool *root_appended, usize *total_sources, usize depth, StrList *src_list
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
				if (src_list) {
					Try(strlist_push(a, src_list, g.gl_pathv[k]) ? Ok(NULL) :
																   Err(ERR_OUT_OF_MEMORY));
				} else {
					strbuf_printf(a, " %s", sh_quote(a, g.gl_pathv[k]));
				}
				(*total_sources)++;
			}
		}
	}

	if (mod->mods) {
		for (usize i = 0; mod->mods[i]; ++i) {
			Try(collect_module_sources(
				a, mod->mods[i], project_dir, full_root_path, root_appended, total_sources,
				depth + 1, src_list
			));
		}
	}

	return Ok(NULL);
}

static void append_profile_flags(Arena *a, const char *profile)
{
	const char *p = (profile && profile[0]) ? profile : "debug";
	if (strcmp(p, "debug") == 0) {
		strbuf_printf(a, " -g");
	} else if (strcmp(p, "release") == 0) {
		strbuf_printf(a, " -O2 -DNDEBUG");
	}
	// Named custom profiles carry no implicit toolchain flags; they surface
	// through NOUR_BUILD_PROFILE / NOUR_PROFILE_<NAME> macros only.
}

static void append_lib_compile_flags(Arena *a, const Library *lib, const char *project_dir)
{
	if (lib->cflags) {
		for (const char **f = lib->cflags; *f; ++f) {
			strbuf_printf(a, " %s", *f);
		}
	}

	strbuf_printf(a, " -I%s", sh_quote(a, rnstr_path(a, project_dir, ".nour", NULL)));

	if (lib->includes) {
		for (const char **inc = lib->includes; *inc; ++inc) {
			strbuf_printf(a, " -I%s", sh_quote(a, rnstr_path(a, project_dir, *inc, NULL)));
		}
	}

	if (lib->public_includes) {
		for (const char **inc = lib->public_includes; *inc; ++inc) {
			strbuf_printf(a, " -I%s", sh_quote(a, rnstr_path(a, project_dir, *inc, NULL)));
		}
	}

	if (lib->mods) {
		for (usize i = 0; lib->mods[i]; ++i) {
			collect_module_includes(a, lib->mods[i], project_dir, 0);
		}
	}

	if (lib->defines) {
		for (const char **d = lib->defines; *d; ++d) {
			strbuf_printf(a, " -D%s", sh_quote(a, *d));
		}
	}

	if (lib->public_defines) {
		for (const char **d = lib->public_defines; *d; ++d) {
			strbuf_printf(a, " -D%s", sh_quote(a, *d));
		}
	}
}

static const char *lib_archive_prefix(const char *name)
{
	return (strncmp(name, "lib", 3) == 0) ? "" : "lib";
}

static void append_public_flags(Arena *a, const DepTransitive *trans, const char *project_dir)
{
	for (usize i = 0; i < trans->num_includes; ++i) {
		strbuf_printf(
			a, " -I%s", sh_quote(a, rnstr_path(a, project_dir, trans->public_includes[i], NULL))
		);
	}
	for (usize i = 0; i < trans->num_defines; ++i) {
		strbuf_printf(a, " -D%s", sh_quote(a, trans->public_defines[i]));
	}
}

static void append_dep_linkargs(
	Arena *a, const DepTransitive *trans, const char *project_dir, const char *build_dir
) {
	// Static archives: dependents before dependencies (trans order).
	for (usize i = 0; i < trans->num_static_libs; ++i) {
		const Library *l = trans->static_libs[i];
		strbuf_printf(
			a, " %s",
			sh_quote(
				a, rnstr_path(
					   a, project_dir, build_dir,
					   rnstr_sprintf(a, "%s%s.a", lib_archive_prefix(l->name), l->name), NULL
				   )
			)
		);
	}

	// Shared libraries: -L + -l, with $ORIGIN rpath for sibling runtime layout.
	if (trans->num_shared_libs > 0) {
		strbuf_printf(a, " -L%s", sh_quote(a, rnstr_path(a, project_dir, build_dir, NULL)));
		for (usize i = 0; i < trans->num_shared_libs; ++i) {
			const Library *l = trans->shared_libs[i];
			const char *lname = (strncmp(l->name, "lib", 3) == 0) ? l->name + 3 : l->name;
			strbuf_printf(a, " -l%s", lname);
		}
		strbuf_printf(a, " -Wl,-rpath,'$ORIGIN'");
	}

	for (usize i = 0; i < trans->num_ldflags; ++i) {
		strbuf_printf(a, " %s", trans->ldflags[i]);
	}
}

static Result build_library(
	const Library *lib, const Project *proj, const char *project_dir, const char *profile,
	const DepTransitive *trans
) {
	const char *cc = (proj->cc && proj->cc[0]) ? proj->cc : "cc";

	// Outputs live under <build_dir>/<profile> so profiles never collide.
	char out_dir_buf[96];
	project_out_dir(out_dir_buf, sizeof(out_dir_buf), proj, profile);
	const char *build_dir = out_dir_buf;

	Try(generate_config_header(proj, (const Target *)lib, project_dir, profile));

	if ((!lib->sources || !lib->sources[0]) && (!lib->mods || !lib->mods[0])) {
		return Err(ERR_TARGET_NO_SOURCES, lib->name);
	}

	arena_scratch(a, 16 * 1024);

	const char *prefix = (strncmp(lib->name, "lib", 3) == 0) ? "" : "lib";
	const char *ext = (lib->type == STATIC) ? ".a" : ".so";
	const char *out_name = rnstr_sprintf(&a, "%s%s%s", prefix, lib->name, ext);
	const char *out_path = rnstr_path(&a, project_dir, build_dir, out_name, NULL);
	const char *obj_dir = rnstr_sprintf(&a, "%s/.nour/obj/%s", project_dir, lib->name);

	if (lib->type == STATIC) {
		Try(fs_create_dir(obj_dir));
	}

	StrList srcs __attribute__((cleanup(strlist_free))) = {0};
	usize	total_sources = 0;
	arena_scratch(fa, 4096);
	const char *compile_flags = NULL;

	strbuf_open(&a);

	if (lib->type == STATIC) {
		strbuf_printf(&a, "cd %s", sh_quote(&a, obj_dir));
	} else if (lib->type == SHARED) {
		strbuf_printf(&a, "%s -shared -fPIC -o %s", cc, sh_quote(&a, out_path));

		append_lib_compile_flags(&a, lib, project_dir);
		append_public_flags(&a, trans, project_dir);
		append_profile_flags(&a, profile);

		// Shared libraries must resolve their own deps at link time; static
		// archives defer everything to the final executable link.
		append_dep_linkargs(&a, trans, project_dir, build_dir);
	} else {
		return Err(ERR_UNKNOWN_TARGET, lib->name);
	}

	if (lib->type == STATIC) {
		// Per-file compiles with unique object names: plain multi-source
		// `cc -c` collides by basename (src/alice.c and tests/alice.c both
		// produce alice.o) and silently loses code. Collect all source
		// paths first (own sources + modules), then compile each one and
		// archive exactly the objects we just produced — stale leftovers
		// from earlier builds never enter the archive.
		// (fa/srcs live at function scope: the archive tail below reads them.)
		strbuf_open(&fa);
		append_lib_compile_flags(&fa, lib, project_dir);
		append_public_flags(&fa, trans, project_dir);
		append_profile_flags(&fa, profile);
		compile_flags = strbuf_finish(&fa);

		if (lib->sources) {
			for (const char **src = lib->sources; *src; ++src) {
				char pattern[PATH_MAX];
				snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

				glob_t g;
				i32 ret = glob(pattern, 0, NULL, &g);
				if (ret) {
					return Err(ERR_TARGET_NO_MATCH, lib->name, *src);
				}
				defer(globfree, &g);

				for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
					Try(strlist_push(&fa, &srcs, g.gl_pathv[k]) ? Ok(NULL) :
																  Err(ERR_OUT_OF_MEMORY));
					++total_sources;
				}
			}
		}

		if (lib->mods) {
			for (usize i = 0; lib->mods[i]; ++i) {
				Try(collect_module_sources(
					&fa, lib->mods[i], project_dir, NULL, NULL, &total_sources, 0, &srcs
				));
			}
		}

		for (usize k = 0; k < srcs.count; ++k) {
			const char *base = strrchr(srcs.items[k], '/');
			base = base ? base + 1 : srcs.items[k];
			const char *obj_name = rnstr_sprintf(&a, "s%zu_%s.o", k, base);
			strbuf_printf(
				&a, " && %s -c %s -o %s %s", cc, sh_quote(&a, srcs.items[k]),
				sh_quote(&a, obj_name), compile_flags
			);
		}
	} else {
		// SHARED: sources compiled straight into the shared object.
		if (lib->sources) {
			for (const char **src = lib->sources; *src; ++src) {
				char pattern[PATH_MAX];
				snprintf(pattern, sizeof(pattern), "%s/%s", project_dir, *src);

				glob_t g;
				i32 ret = glob(pattern, 0, NULL, &g);
				if (ret) {
					return Err(ERR_TARGET_NO_MATCH, lib->name, *src);
				}
				defer(globfree, &g);

				for (usize k = 0; k < (usize)g.gl_pathc; ++k) {
					strbuf_printf(&a, " %s", sh_quote(&a, g.gl_pathv[k]));
					++total_sources;
				}
			}
		}

		if (lib->mods) {
			for (usize i = 0; lib->mods[i]; ++i) {
				Try(collect_module_sources(
					&a, lib->mods[i], project_dir, NULL, NULL, &total_sources, 0, NULL
				));
			}
		}
	}

	if (!total_sources) {
		return Err(ERR_TARGET_ZERO_SOURCES, lib->name);
	}

	if (lib->type == STATIC) {
		// Archive exactly the objects produced above (never stale leftovers),
		// with explicit names — no basename collisions possible.
		strbuf_printf(&a, " && ar rcs %s", sh_quote(&a, out_path));
		for (usize k = 0; k < srcs.count; ++k) {
			const char *base = strrchr(srcs.items[k], '/');
			base = base ? base + 1 : srcs.items[k];
			strbuf_printf(&a, " %s", sh_quote(&a, rnstr_sprintf(&a, "s%zu_%s.o", k, base)));
		}
		strbuf_printf(&a, " && rm -f *.o");
	} else if (lib->ldflags) {
		for (const char **ld = lib->ldflags; *ld; ++ld) {
			strbuf_printf(&a, " %s", *ld);
		}
	}

	const char *cmd = strbuf_finish(&a);
	i32 status = system(cmd);
	if (status) {
		return Err(ERR_BUILD_FAILED, lib->name);
	}

	return Ok(NULL);
}

static Result build_executable(
	const Executable *exe, const Project *proj, const char *project_dir, const char *profile,
	const DepTransitive *trans
) {
	const char *cc = (proj->cc && proj->cc[0]) ? proj->cc : "cc";

	// Outputs live under <build_dir>/<profile> so profiles never collide.
	char out_dir_buf[96];
	project_out_dir(out_dir_buf, sizeof(out_dir_buf), proj, profile);
	const char *build_dir = out_dir_buf;

	Try(generate_config_header(proj, (const Target *)exe, project_dir, profile));

	const char *root = resolve_executable_root(exe, project_dir);
	if (exe->root && !root) {
		return Err(ERR_TARGET_NO_ROOT, exe->name, exe->root);
	}

	arena_scratch(a, 16 * 1024);

	const char *full_root_path = root ? rnstr_sprintf(&a, "%s/%s", project_dir, root) : NULL;
	bool root_appended = false;

	const char *out_path = rnstr_path(&a, project_dir, build_dir, exe->name, NULL);

	strbuf_open(&a);
	strbuf_printf(&a, "%s -o %s", cc, sh_quote(&a, out_path));

	// Cflags
	if (exe->cflags) {
		for (const char **f = exe->cflags; *f; ++f) {
			strbuf_printf(&a, " %s", *f);
		}
	}

	// Generated header include path (.nour)
	strbuf_printf(&a, " -I%s", sh_quote(&a, rnstr_path(&a, project_dir, ".nour", NULL)));

	// Includes
	if (exe->includes) {
		for (const char **inc = exe->includes; *inc; ++inc) {
			strbuf_printf(&a, " -I%s", sh_quote(&a, rnstr_path(&a, project_dir, *inc, NULL)));
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
			strbuf_printf(&a, " -D%s", sh_quote(&a, *d));
		}
	}

	// Transitive include dirs / defines from dependencies.
	append_public_flags(&a, trans, project_dir);

	// Profile flags (debug -> -g, release -> -O2 -DNDEBUG).
	append_profile_flags(&a, profile);

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
				strbuf_printf(&a, " %s", sh_quote(&a, g.gl_pathv[k]));
				++total_sources;
			}
		}
	}

	// Module sources
	if (exe->mods) {
		for (usize i = 0; exe->mods[i]; ++i) {
			Try(collect_module_sources(
				&a, exe->mods[i], project_dir, full_root_path, &root_appended, &total_sources, 0,
				NULL
			));
		}
	}

	// If root wasn't already in sources glob, append it
	if (root && !root_appended) {
		strbuf_printf(&a, " %s", sh_quote(&a, full_root_path));
		++total_sources;
	}

	if (!total_sources) {
		return Err(ERR_TARGET_ZERO_SOURCES, exe->name);
	}

	// Link flags
	if (exe->ldflags) {
		for (const char **ld = exe->ldflags; *ld; ++ld) {
			strbuf_printf(&a, " %s", *ld);
		}
	}

	// Dependency archives / shared libs / propagated ldflags.
	append_dep_linkargs(&a, trans, project_dir, build_dir);

	// Execute build
	const char *cmd = strbuf_finish(&a);
	i32 status = system(cmd);
	if (status) {
		return Err(ERR_BUILD_FAILED, exe->name);
	}

	return Ok(NULL);
}

Result build_target(
	const Project *proj, const char *project_dir, const char *target_name, const char *profile
) {
	if (!proj->targets) {
		return Ok(NULL);
	}

	// Profiles label directories and shell commands — forbid anything
	// that could inject into either.
	if (profile && profile[0] && !is_valid_profile(profile)) {
		return Err("invalid profile name '%s'", profile);
	}

	// Normalize to an absolute path: per-object compiles `cd` into .nour/obj,
	// so cwd-relative source paths ("./src/x.c") would break afterwards.
	char abs_dir[PATH_MAX];
	char resolved[PATH_MAX];
	if (realpath(project_dir, resolved)) {
		project_dir = resolved;
	} else if (snprintf(abs_dir, sizeof(abs_dir), "%s", project_dir) >= 0) {
		project_dir = abs_dir; // keep original if the dir vanished mid-run
	}

	char prof_buf[96];
	project_out_dir(prof_buf, sizeof(prof_buf), proj, profile);

	const char *build_dir = prof_buf;
	arena_scratch(a, 256);
	const char *build_path = rnstr_sprintf(&a, "%s/%s", project_dir, build_dir);
	Result r = fs_create_dir(build_path);
	Try(r);

	DepPlan plan = {0};
	Try(deps_resolve(proj, target_name, &a, &plan));

	for (usize i = 0; i < plan.count; ++i) {
		Target *target = plan.targets[i];
		{
			// ta owns the transitive flag data for this build; it must
			// outlive the build_c call below.
			arena_scratch(ta, 4096);
			DepTransitive trans = {0};
			Try(deps_collect_transitive(proj, target, &ta, &trans));

			if (target->kind == T_EXECUTABLE) {
				Try(build_executable((Executable *)target, proj, project_dir, profile, &trans));
			} else if (target->kind == T_LIBRARY) {
				Try(build_library((Library *)target, proj, project_dir, profile, &trans));
			} else {
				return Err(ERR_UNKNOWN_TARGET, target->name);
			}
		}
	}

	return Ok(NULL);
}

Result build_project(const Project *proj, const char *project_dir)
{
	return build_target(proj, project_dir, NULL, "debug");
}
