#include "test.h"
#include "builder.h"
#include "nour.h"
#include "types.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_test_file(const char *dir, const char *relpath, const char *content)
{
	char full[512];
	snprintf(full, sizeof(full), "%s/%s", dir, relpath);

	char *slash = strrchr(full, '/');
	if (slash) {
		*slash = '\0';
		char mkdir_cmd[600];
		snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", full);
		(void)system(mkdir_cmd);
		*slash = '/';
	}

	FILE *f = fopen(full, "w");
	if (!f)
		abort();
	defer(fclose, f);
	fputs(content, f);
}

TEST(Builder, build_executable_with_explicit_root)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/entry.c", "int calc(void); int main(void) { return calc(); }\n");
	write_test_file(td.path, "src/calc.c", "int calc(void) { return 0; }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "my_app",
		.root	 = "src/entry.c",
		.sources = (const char *[]){ "src/calc.c", NULL },
	};
	Project proj = {
		.name	   = "AppProject",
		.build_dir = "build",
		.targets   = (void *[]){ &exe, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/build/my_app", td.path);
	ASSERT_TRUE(access(bin_path, F_OK) == 0);

	char run_cmd[600];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, build_executable_default_root_fallback)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/main.c", "int helper(void); int main(void) { return helper(); }\n");
	write_test_file(td.path, "src/helper.c", "int helper(void) { return 0; }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "fallback_app",
		.root	 = NULL,
		.sources = (const char *[]){ "src/helper.c", NULL },
	};
	Project proj = {
		.name	   = "FallbackProject",
		.build_dir = "out",
		.targets   = (void *[]){ &exe, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/out/fallback_app", td.path);
	ASSERT_TRUE(access(bin_path, F_OK) == 0);

	char run_cmd[600];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, build_executable_root_in_sources_dedup)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/main.c", "int extra(void); int main(void) { return extra(); }\n");
	write_test_file(td.path, "src/extra.c", "int extra(void) { return 0; }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "dedup_app",
		.root	 = "src/main.c",
		.sources = (const char *[]){ "src/*.c", NULL },
	};
	Project proj = {
		.name	   = "DedupProject",
		.build_dir = "bin",
		.targets   = (void *[]){ &exe, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/bin/dedup_app", td.path);
	ASSERT_TRUE(access(bin_path, F_OK) == 0);

	char run_cmd[600];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, build_executable_missing_root_error)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/foo.c", "int foo(void) { return 0; }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "missing_root_app",
		.root	 = "src/non_existent.c",
		.sources = (const char *[]){ "src/foo.c", NULL },
	};
	Project proj = {
		.name	 = "BadRootProject",
		.targets = (void *[]){ &exe, NULL },
	};

	ASSERT_RESULT_ERR_CONTAINS(
		build_project(&proj, td.path), "root file 'src/non_existent.c' not found"
	);
}

TEST(Builder, build_executable_no_sources_or_root_error)
{
	TMPDIR_AUTO(td);

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "empty_app",
		.root	 = NULL,
		.sources = NULL,
	};
	Project proj = {
		.name	 = "EmptyProject",
		.targets = (void *[]){ &exe, NULL },
	};

	ASSERT_RESULT_ERR_CONTAINS(build_project(&proj, td.path), "has no sources specified");
}

TEST(Builder, build_executable_glob_no_match_error)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/main.c", "int main(void) { return 0; }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "bad_glob_app",
		.root	 = "src/main.c",
		.sources = (const char *[]){ "src/missing_*.c", NULL },
	};
	Project proj = {
		.name	 = "BadGlobProject",
		.targets = (void *[]){ &exe, NULL },
	};

	ASSERT_RESULT_ERR_CONTAINS(
		build_project(&proj, td.path), "no files found matching 'src/missing_*.c'"
	);
}

TEST(Builder, build_executable_compilation_failure)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/main.c", "int main(void) { this is totally broken syntax }\n");

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "broken_app",
		.root	 = "src/main.c",
		.sources = NULL,
	};
	Project proj = {
		.name	 = "BrokenProject",
		.cc		 = "cc 2>/dev/null",
		.targets = (void *[]){ &exe, NULL },
	};

	ASSERT_RESULT_ERR_CONTAINS(build_project(&proj, td.path), "Build failed for target 'broken_app'");
}

TEST(Builder, build_static_library)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/add.c", "int add(int a, int b) { return a + b; }\n");
	write_test_file(td.path, "src/sub.c", "int sub(int a, int b) { return a - b; }\n");

	Library lib = {
		.kind	 = T_LIBRARY,
		.name	 = "calc",
		.type	 = STATIC,
		.sources = (const char *[]){ "src/*.c", NULL },
	};
	Project proj = {
		.name	   = "CalcLibProj",
		.build_dir = "build",
		.targets   = (void *[]){ &lib, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char lib_path[512];
	snprintf(lib_path, sizeof(lib_path), "%s/build/libcalc.a", td.path);
	ASSERT_TRUE(access(lib_path, F_OK) == 0);

	// Test linking against the produced archive
	write_test_file(
		td.path, "src/main.c",
		"int add(int, int); int sub(int, int);\n"
		"int main(void) { return (add(2, 3) == 5 && sub(5, 3) == 2) ? 0 : 1; }\n"
	);
	char compile_cmd[1024];
	snprintf(
		compile_cmd, sizeof(compile_cmd), "cc \"%s/src/main.c\" \"%s\" -o \"%s/build/test_app\"",
		td.path, lib_path, td.path
	);
	ASSERT_EQ(system(compile_cmd), 0);

	char run_cmd[512];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s/build/test_app\"", td.path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, build_shared_library)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "src/ops.c", "int multiply(int a, int b) { return a * b; }\n");

	Library lib = {
		.kind	 = T_LIBRARY,
		.name	 = "ops",
		.type	 = SHARED,
		.sources = (const char *[]){ "src/ops.c", NULL },
	};
	Project proj = {
		.name	   = "OpsLibProj",
		.build_dir = "build",
		.targets   = (void *[]){ &lib, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char lib_path[512];
	snprintf(lib_path, sizeof(lib_path), "%s/build/libops.so", td.path);
	ASSERT_TRUE(access(lib_path, F_OK) == 0);

	// Load shared library and call its function
	void *h = dlopen(lib_path, RTLD_NOW);
	ASSERT_TRUE(h != NULL);
	typedef int (*OpsMulFn)(int, int);
	OpsMulFn mul = (OpsMulFn)dlsym(h, "multiply");
	ASSERT_TRUE(mul != NULL);
	ASSERT_EQ(mul(6, 7), 42);
	dlclose(h);
}

TEST(Builder, build_library_with_modules)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "core/core.c", "int core_fn(void) { return 42; }\n");
	write_test_file(
		td.path, "src/api.c", "int core_fn(void); int api_fn(void) { return core_fn(); }\n"
	);

	Module core_mod = {
		.name	 = "core",
		.sources = (const char *[]){ "core/*.c", NULL },
	};
	Library lib = {
		.kind	 = T_LIBRARY,
		.name	 = "mylib",
		.type	 = STATIC,
		.sources = (const char *[]){ "src/api.c", NULL },
		.mods	 = (Module *[]){ &core_mod, NULL },
	};
	Project proj = {
		.name	   = "ModLibProj",
		.build_dir = "build",
		.targets   = (void *[]){ &lib, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char lib_path[512];
	snprintf(lib_path, sizeof(lib_path), "%s/build/libmylib.a", td.path);
	ASSERT_TRUE(access(lib_path, F_OK) == 0);

	// Test linking against archive and calling module-dependent function
	write_test_file(
		td.path, "src/main.c",
		"int api_fn(void);\n"
		"int main(void) { return (api_fn() == 42) ? 0 : 1; }\n"
	);
	char compile_cmd[1024];
	snprintf(
		compile_cmd, sizeof(compile_cmd), "cc \"%s/src/main.c\" \"%s\" -o \"%s/build/test_app\"",
		td.path, lib_path, td.path
	);
	ASSERT_EQ(system(compile_cmd), 0);

	char run_cmd[512];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s/build/test_app\"", td.path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, generated_config_header_and_profiles)
{
	TMPDIR_AUTO(td);

	const char *main_c =
		"#include <nour/config.h>\n"
		"#include <string.h>\n"
		"int main(void) {\n"
		"    if (strcmp(NOUR_PROJECT_NAME, \"ConfigApp\") != 0) return 1;\n"
		"    if (strcmp(NOUR_PROJECT_VERSION, \"2.3.4\") != 0) return 2;\n"
		"    if (NOUR_VERSION_MAJOR != 2 || NOUR_VERSION_MINOR != 3 || NOUR_VERSION_PATCH != 4) return 3;\n"
		"    if (strcmp(NOUR_TARGET_NAME, \"config_bin\") != 0) return 4;\n"
		"    if (strcmp(NOUR_BUILD_PROFILE, \"staging\") != 0) return 5;\n"
		"#ifndef NOUR_PROFILE_STAGING\n"
		"    return 6;\n"
		"#endif\n"
		"    return 0;\n"
		"}\n";

	write_test_file(td.path, "src/main.c", main_c);

	Executable exe = {
		.kind	 = T_EXECUTABLE,
		.name	 = "config_bin",
		.root	 = "src/main.c",
		.sources = NULL,
	};
	Project proj = {
		.name	   = "ConfigApp",
		.version   = "2.3.4",
		.build_dir = "build",
		.targets   = (void *[]){ &exe, NULL },
	};

	Result res = build_project_target(&proj, td.path, "config_bin", "staging");
	ASSERT_RESULT_OK(res);

	// Verify header was generated on disk
	char header_path[512];
	snprintf(header_path, sizeof(header_path), "%s/.nour/nour/config.h", td.path);
	ASSERT_TRUE(access(header_path, F_OK) == 0);

	// Verify compiled binary runs and all assertions in main() passed
	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/build/config_bin", td.path);
	ASSERT_TRUE(access(bin_path, F_OK) == 0);

	char run_cmd[600];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);
	ASSERT_EQ(system(run_cmd), 0);
}

TEST(Builder, build_executable_with_modules)
{
	TMPDIR_AUTO(td);

	write_test_file(td.path, "nested/deep.h", "int deep_val(void);\n");
	write_test_file(
		td.path, "nested/deep.c", "#include \"deep.h\"\nint deep_val(void) { return 40; }\n"
	);

	write_test_file(td.path, "sub/calc.h", "int add(int a, int b);\n");
	write_test_file(
		td.path, "sub/calc.c", "#include \"calc.h\"\nint add(int a, int b) { return a + b; }\n"
	);

	write_test_file(
		td.path, "src/main.c",
		"#include \"calc.h\"\n"
		"#include \"deep.h\"\n"
		"int main(void) {\n"
		"    return (add(1, 1) + deep_val() == 42) ? 0 : 1;\n"
		"}\n"
	);

	Module deep_mod = {
		.name	  = "deep_mod",
		.sources  = (const char *[]){ "nested/*.c", NULL },
		.includes = (const char *[]){ "nested", NULL },
	};

	Module calc_mod = {
		.name	  = "calc_mod",
		.sources  = (const char *[]){ "sub/*.c", NULL },
		.includes = (const char *[]){ "sub", NULL },
		.mods	  = (Module *[]){ &deep_mod, NULL },
	};

	Executable exe = {
		.kind = T_EXECUTABLE,
		.name = "mod_app",
		.root = "src/main.c",
		.mods = (Module *[]){ &calc_mod, NULL },
	};

	Project proj = {
		.name	   = "ModProject",
		.build_dir = "bin",
		.targets   = (void *[]){ &exe, NULL },
	};

	Result res = build_project(&proj, td.path);
	ASSERT_RESULT_OK(res);

	char bin_path[512];
	snprintf(bin_path, sizeof(bin_path), "%s/bin/mod_app", td.path);
	ASSERT_TRUE(access(bin_path, F_OK) == 0);

	char run_cmd[600];
	snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin_path);
	ASSERT_EQ(system(run_cmd), 0);
}
