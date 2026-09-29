#pragma once

#include "nour.h"

static inline const char *project_build_dir(const Project *proj)
{
	return (proj && proj->build_dir && proj->build_dir[0]) ? proj->build_dir : "build";
}

void print_project(const Project *proj);
