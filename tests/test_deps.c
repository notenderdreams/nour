#include "test.h"
#include "deps.h"
#include "error.h"
#include "nour.h"
#include <string.h>

TEST(Deps, single_target_no_deps)
{
	arena_scratch(a, 4096);

	Executable exe = {
		.kind = T_EXECUTABLE,
		.name = "my_app",
		.deps = NULL,
	};
	Project proj = {
		.name	 = "TestProj",
		.targets = (void *[]){&exe, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_RESULT_OK(r);
	ASSERT_EQ(plan.count, 1);
	ASSERT_STR_EQ(plan.targets[0]->name, "my_app");
}

TEST(Deps, linear_dependency_chain)
{
	arena_scratch(a, 4096);

	Library lib_b = {
		.kind = T_LIBRARY,
		.name = "libB",
		.type = STATIC,
		.deps = NULL,
	};
	Library lib_a = {
		.kind = T_LIBRARY,
		.name = "libA",
		.type = STATIC,
		.deps = (void *[]){&lib_b, NULL},
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&lib_a, NULL},
	};
	Project proj = {
		.name	 = "TestProj",
		.targets = (void *[]){&app, &lib_a, &lib_b, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_RESULT_OK(r);
	ASSERT_EQ(plan.count, 3);
	// libB must build before libA, and libA before app
	ASSERT_STR_EQ(plan.targets[0]->name, "libB");
	ASSERT_STR_EQ(plan.targets[1]->name, "libA");
	ASSERT_STR_EQ(plan.targets[2]->name, "app");
}

TEST(Deps, diamond_dependency_graph)
{
	arena_scratch(a, 4096);

	Library lib_d = {
		.kind = T_LIBRARY,
		.name = "libD",
		.type = STATIC,
		.deps = NULL,
	};
	Library lib_b = {
		.kind = T_LIBRARY,
		.name = "libB",
		.type = STATIC,
		.deps = (void *[]){&lib_d, NULL},
	};
	Library lib_c = {
		.kind = T_LIBRARY,
		.name = "libC",
		.type = STATIC,
		.deps = (void *[]){&lib_d, NULL},
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&lib_b, &lib_c, NULL},
	};
	Project proj = {
		.name	 = "DiamondProj",
		.targets = (void *[]){&app, &lib_b, &lib_c, &lib_d, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_RESULT_OK(r);
	ASSERT_EQ(plan.count, 4);

	// libD must be first
	ASSERT_STR_EQ(plan.targets[0]->name, "libD");
	// libB and libC in positions 1 and 2
	bool has_b =
		(strcmp(plan.targets[1]->name, "libB") == 0 || strcmp(plan.targets[2]->name, "libB") == 0);
	bool has_c =
		(strcmp(plan.targets[1]->name, "libC") == 0 || strcmp(plan.targets[2]->name, "libC") == 0);
	ASSERT_TRUE(has_b);
	ASSERT_TRUE(has_c);
	// app must be last
	ASSERT_STR_EQ(plan.targets[3]->name, "app");
}

TEST(Deps, circular_dependency_cycle)
{
	arena_scratch(a, 4096);

	Library lib_a = {
		.kind = T_LIBRARY,
		.name = "libA",
		.type = STATIC,
	};
	Library lib_b = {
		.kind = T_LIBRARY,
		.name = "libB",
		.type = STATIC,
		.deps = (void *[]){&lib_a, NULL},
	};
	// Close cycle: libA -> libB
	lib_a.deps = (void *[]){&lib_b, NULL};

	Project proj = {
		.name	 = "CycleProj",
		.targets = (void *[]){&lib_a, &lib_b, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_TRUE(strstr(r.error, "circular dependency detected:") != NULL);
	ASSERT_TRUE(strstr(r.error, "libA") != NULL);
	ASSERT_TRUE(strstr(r.error, "libB") != NULL);
	result_free(&r);
}

TEST(Deps, circular_self_dependency)
{
	arena_scratch(a, 4096);

	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
	};
	app.deps = (void *[]){&app, NULL};

	Project proj = {
		.name	 = "SelfCycleProj",
		.targets = (void *[]){&app, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_TRUE(strstr(r.error, "circular dependency detected: app -> app") != NULL);
	result_free(&r);
}

TEST(Deps, selective_target_resolution)
{
	arena_scratch(a, 4096);

	Library lib_core = {
		.kind = T_LIBRARY,
		.name = "core",
		.type = STATIC,
		.deps = NULL,
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&lib_core, NULL},
	};
	Library lib_unused = {
		.kind = T_LIBRARY,
		.name = "unused_lib",
		.type = STATIC,
		.deps = NULL,
	};
	Executable unused_app = {
		.kind = T_EXECUTABLE,
		.name = "unused_app",
		.deps = (void *[]){&lib_unused, NULL},
	};

	Project proj = {
		.name	 = "MultiProj",
		.targets = (void *[]){&app, &unused_app, &lib_core, &lib_unused, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, "app", &a, &plan);
	ASSERT_RESULT_OK(r);
	ASSERT_EQ(plan.count, 2);
	ASSERT_STR_EQ(plan.targets[0]->name, "core");
	ASSERT_STR_EQ(plan.targets[1]->name, "app");
}

TEST(Deps, target_not_found)
{
	arena_scratch(a, 4096);

	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = NULL,
	};
	Project proj = {
		.name	 = "NotFoundProj",
		.targets = (void *[]){&app, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, "non_existent", &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_STR_EQ(r.error, "Target 'non_existent' not found in project 'NotFoundProj'");
	result_free(&r);
}

TEST(Deps, invalid_dependency_pointer)
{
	arena_scratch(a, 4096);

	Target unknown = {
		.kind = T_LIBRARY,
		.name = "ghost",
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&unknown, NULL},
	};
	Project proj = {
		.name	 = "InvalidProj",
		.targets = (void *[]){&app, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_STR_EQ(r.error, "target 'app' has invalid dependency 'ghost'");
	result_free(&r);
}

TEST(Deps, duplicate_target_names_rejected)
{
	arena_scratch(a, 4096);

	Library lib_a = {.kind = T_LIBRARY, .name = "dup", .type = STATIC};
	Library lib_b = {.kind = T_LIBRARY, .name = "dup", .type = STATIC};
	Executable app = {
		  .kind = T_EXECUTABLE,
		  .name = "app",
	  };
	Project proj = {
		.name	 = "DupNamesProj",
		.targets = (void *[]){&app, &lib_a, &lib_b, NULL},
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_STR_EQ(r.error, "duplicate target name 'dup'");
	result_free(&r);
}

TEST(Deps, fat_public_include_list)
{
	arena_scratch(a, 4096);

	// A library with far more public entries than any per-node heuristic:
	// collection arrays must be sized exactly, not capped — the original
	// `count * 16` cap silently corrupted neighbouring arrays.
	static char inc_bufs[100][16];
	const char **incs = arena_alloc(&a, 101 * sizeof(const char *));
	for (usize i = 0; i < 100; ++i) {
		snprintf(inc_bufs[i], sizeof(inc_bufs[i]), "inc%zu", i);
		incs[i] = inc_bufs[i];
	}
	incs[100] = NULL;

	Library lib_b = {
		.kind			= T_LIBRARY,
		.name			= "fat_b",
		.type			= STATIC,
		.public_defines = (const char *[]){"D1", "D2", "D3", NULL},
	};
	Library lib_fat = {
		.kind			 = T_LIBRARY,
		.name			 = "fat",
		.type			 = STATIC,
		.public_includes = incs,
		.deps			 = (void *[]){&lib_b, NULL},
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&lib_fat, NULL},
	};
	Project proj = {
		.name	 = "FatProj",
		.targets = (void *[]){&app, &lib_fat, &lib_b, NULL},
	};

	DepTransitive trans = {0};
	Result r = deps_collect_transitive(&proj, (const Target *)&app, &a, &trans);
	ASSERT_RESULT_OK(r);

	// All 100 includes arrived intact
	ASSERT_EQ(trans.num_includes, 100);
	ASSERT_STR_EQ(trans.public_includes[0], "inc0");
	ASSERT_STR_EQ(trans.public_includes[99], "inc99");

	// And the neighbouring collection was not corrupted by an overrun
	ASSERT_EQ(trans.num_defines, 3);
	ASSERT_STR_EQ(trans.public_defines[0], "D1");
	ASSERT_STR_EQ(trans.public_defines[2], "D3");
}

TEST(Deps, graph_depth_guard)
{
	arena_scratch(a, 4 * 1024 * 1024);

	// 4111-node chain: recursion is bounded by the guard, so we get a clear
	// diagnostic instead of blowing the real C stack.
	enum { DEEP_N = 4111 };
	Executable *nodes = arena_alloc(&a, DEEP_N * sizeof(Executable));
	static char names[DEEP_N][16];
	for (usize i = 0; i < DEEP_N; ++i) {
		snprintf(names[i], sizeof(names[i]), "n%zu", i);
		nodes[i].kind = T_EXECUTABLE;
		nodes[i].name = names[i];
		if (i + 1 < DEEP_N) {
			// Compound literals inside the loop body have iteration
			// lifetime — allocate the edge arrays on the arena instead.
			void **dep = arena_alloc(&a, 2 * sizeof(void *));
			dep[0] = &nodes[i + 1];
			dep[1] = NULL;
			nodes[i].deps = dep;
		} else {
			nodes[i].deps = NULL;
		}
	}

	void **targets = arena_alloc(&a, (DEEP_N + 1) * sizeof(void *));
	for (usize i = 0; i < DEEP_N; ++i)
		targets[i] = &nodes[i];
	targets[DEEP_N] = NULL;

	Project proj = {
		.name	 = "DeepProj",
		.targets = targets,
	};

	DepPlan plan = {0};
	Result	r = deps_resolve(&proj, NULL, &a, &plan);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_STR_CONTAINS(r.error, "dependency depth limit exceeded");
	result_free(&r);
}

TEST(Deps, collect_transitive_flags_and_libs)
{
	arena_scratch(a, 4096);

	Library lib_b = {
		.kind			 = T_LIBRARY,
		.name			 = "libB",
		.type			 = STATIC,
		.public_includes = (const char *[]){"libB/include", NULL},
		.public_defines	 = (const char *[]){"USE_LIB_B=1", NULL},
		.ldflags		 = (const char *[]){"-lm", NULL},
		.deps			 = NULL,
	};
	Library lib_a = {
		.kind			 = T_LIBRARY,
		.name			 = "libA",
		.type			 = STATIC,
		.public_includes = (const char *[]){"libA/include", "libB/include", NULL}, // test dedup
		.public_defines	 = (const char *[]){"USE_LIB_A=1", NULL},
		.deps			 = (void *[]){&lib_b, NULL},
	};
	Executable app = {
		.kind = T_EXECUTABLE,
		.name = "app",
		.deps = (void *[]){&lib_a, NULL},
	};
	Project proj = {
		.name	 = "FlagsProj",
		.targets = (void *[]){&app, &lib_a, &lib_b, NULL},
	};

	DepTransitive trans = {0};
	Result r = deps_collect_transitive(&proj, (const Target *)&app, &a, &trans);
	ASSERT_RESULT_OK(r);

	// Includes deduplicated: libA/include, libB/include
	ASSERT_EQ(trans.num_includes, 2);
	ASSERT_STR_EQ(trans.public_includes[0], "libA/include");
	ASSERT_STR_EQ(trans.public_includes[1], "libB/include");

	// Defines: USE_LIB_A=1, USE_LIB_B=1
	ASSERT_EQ(trans.num_defines, 2);
	ASSERT_STR_EQ(trans.public_defines[0], "USE_LIB_A=1");
	ASSERT_STR_EQ(trans.public_defines[1], "USE_LIB_B=1");

	// Static libs in link order: libA before libB (dependents before dependencies)
	ASSERT_EQ(trans.num_static_libs, 2);
	ASSERT_STR_EQ(trans.static_libs[0]->name, "libA");
	ASSERT_STR_EQ(trans.static_libs[1]->name, "libB");

	// Ldflags: -lm
	ASSERT_EQ(trans.num_ldflags, 1);
	ASSERT_STR_EQ(trans.ldflags[0], "-lm");
}
