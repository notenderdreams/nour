#pragma once

#include "nour.h"
#include "types.h"

Result build_project(const Project *proj, const char *project_dir);
Result build_project_target(const Project *proj, const char *project_dir, const char *target_name);
