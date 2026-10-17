#include "utils.h"
#include "str.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

const char *section_banner(const char *name)
{
	static _Thread_local char buf[256];
	snprintf(buf, sizeof(buf), "\n// ─────────────────── %s ───────────────────\n", name);
	return buf;
}

/* Profile names label directories and shell commands: allow only safe
 * identifier-ish characters so they can never inject into either. */
bool is_valid_profile(const char *name)
{
	if (!name || !name[0])
		return false;
	for (const char *p = name; *p; ++p) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
			  *p == '_' || *p == '-')) {
			return false;
		}
	}
	return true;
}

bool is_valid_name(const char *name)
{

	if (!name || (!isalpha((unsigned char)*name) && *name != '_'))
		return false;
	for (const char *p = name + 1; *p; ++p) {
		if (!isalnum((unsigned char)*p) && *p != '_')
			return false;
	}
	return true;
}

void parse_semver(const char *version, i32 *major, i32 *minor, i32 *patch)
{
	*major = 0;
	*minor = 0;
	*patch = 0;
	if (!version || !*version)
		return;

	StrView sv = sv_trim(sv_fromc(version));
	if (sv_starts(sv, SV("v")) || sv_starts(sv, SV("V"))) {
		sv = sv_sub(sv, 1, sv.len - 1);
	}

	StrView maj_sv, rem1;
	if (sv_split(sv, '.', &maj_sv, &rem1)) {
		*major = (i32)strtol(maj_sv.data, NULL, 10);
		StrView min_sv, patch_sv;
		if (sv_split(rem1, '.', &min_sv, &patch_sv)) {
			*minor = (i32)strtol(min_sv.data, NULL, 10);
			*patch = (i32)strtol(patch_sv.data, NULL, 10);
		} else {
			*minor = (i32)strtol(rem1.data, NULL, 10);
		}
	} else {
		*major = (i32)strtol(sv.data, NULL, 10);
	}
}

void sanitize_macro_name(const char *src, char *dst, usize cap)
{
	if (!src || !dst || cap == 0)
		return;
	usize i = 0;
	for (; src[i] && i < cap - 1; ++i) {
		char c = src[i];
		dst[i] = (c == '-' || c == '.' || c == ' ') ? '_' : toupper((unsigned char)c);
	}
	dst[i] = '\0';
}

const char *sh_quote(Arena *a, const char *s)
{
	if (!s)
		s = "";
	usize n = strlen(s);

	// Worst case: every byte is a quote, each expanded to '\'' (4 bytes).
	char *out = arena_alloc_align(a, n * 4 + 4, 1);
	if (!out)
		return "''";

	char *p = out;
	*p++	= '\'';
	for (usize i = 0; i < n; ++i) {
		if (s[i] == '\'') {
			memcpy(p, "'\\''", 4);
			p += 4;
		} else {
			*p++ = s[i];
		}
	}
	*p++ = '\'';
	*p	 = '\0';
	return out;
}
