#pragma once

#include "arena.h"
#include "nour.h"
#include "types.h"

#include <stdio.h>

static inline const char *project_build_dir(const Project *proj)
{
	return (proj && proj->build_dir && proj->build_dir[0]) ? proj->build_dir : "build";
}

/* Directory where a profile's build outputs live: <build_dir>/<profile>.
 * Nested per-profile so a --release build never clobbers a debug binary.
 * `profile` NULL/empty means the default profile (debug). */
static inline usize project_out_dir(char *dst, usize cap, const Project *proj, const char *profile)
{
	const char *base = project_build_dir(proj);
	const char *prof = (profile && profile[0]) ? profile : "debug";
	return (usize)snprintf(dst, cap, "%s/%s", base, prof);
}

const char *section_banner(const char *name);
bool		is_valid_name(const char *name);
bool		is_valid_profile(const char *name);
const char *sh_quote(Arena *a, const char *s);

void parse_semver(const char *version, i32 *major, i32 *minor, i32 *patch);
void sanitize_macro_name(const char *src, char *dst, usize cap);
