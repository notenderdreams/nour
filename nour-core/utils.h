#pragma once

#include "nour.h"
#include "types.h"

static inline const char *project_build_dir(const Project *proj)
{
	return (proj && proj->build_dir && proj->build_dir[0]) ? proj->build_dir : "build";
}

const char *section_banner(const char *name);
bool		is_valid_name(const char *name);
