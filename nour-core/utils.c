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
