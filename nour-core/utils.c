#include "utils.h"
#include <ctype.h>
#include <stdio.h>

const char *section_banner(const char *name)
{
	static _Thread_local char buf[256];
	snprintf(buf, sizeof(buf), "\n// ─────────────────── %s ───────────────────\n", name);
	return buf;
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
	if (!version)
		return;

	sscanf(version, "%d.%d.%d", major, minor, patch);
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
