#pragma once

#include "arena.h"
#include "nour.h"
#include "types.h"

typedef struct {
	Target **targets;
	usize	 count;
} DepPlan;

typedef struct {
	const char	  **public_includes;
	usize			num_includes;
	const char	  **public_defines;
	usize			num_defines;
	const Library **static_libs;
	usize			num_static_libs;
	const Library **shared_libs;
	usize			num_shared_libs;
	const char	  **ldflags;
	usize			num_ldflags;
} DepTransitive;

Result deps_resolve(const Project *proj, const char *target_name, Arena *a, DepPlan *out_plan);
Result deps_collect_transitive(
	const Project *proj, const Target *target, Arena *a, DepTransitive *out_trans
);
