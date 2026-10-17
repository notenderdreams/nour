#include "test.h"
#include "parser.h"
#include "arena.h"
#include <sys/stat.h>

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

TEST(Preprocessor, root_rewritten_in_included_manifest)
{
	TMPDIR_AUTO(td);

	// A manifest included from a subdirectory: its scalar `.root` must be
	// rewritten to stay relative to the top-level project, exactly like
	// .sources/.includes arrays already are.
	write_test_file(td.path, "sub/mod.nour", "Executable app = {\n    .root = \"main.c\",\n};\n");
	write_test_file(
		td.path, "project.nour", "#include \"sub/mod.nour\"\nProject P = { .targets = { &app } };\n"
	);

	arena_scratch(a, 4096);
	const char *in_path = rnstr_sprintf(&a, "%s/project.nour", td.path);
	const char *out_path = rnstr_sprintf(&a, "%s/out.c", td.path);

	ASSERT_RESULT_OK(preprocess(in_path, out_path));

	char content[8192] = {0};
	FILE *f = fopen(out_path, "r");
	ASSERT_NOT_NULL(f);
	defer(fclose, f);
	usize n = fread(content, 1, sizeof(content) - 1, f);
	content[n] = '\0';

	ASSERT_STR_CONTAINS(content, ".root = \"sub/main.c\",");
}

TEST(Preprocessor, invalid_declaration_name_rejected)
{
	char *out __attribute__((cleanup(tf_free_str))) = NULL;

	Result res = preprocess_str(
		"Executable my-app = {\n"
		"    .root = \"src/main.c\",\n"
		"};\n"
		"Project BadName = { .targets = { &app } };\n",
		&out
	);
	ASSERT_FALSE(res.ok);
	ASSERT_TRUE(res.error != NULL);
	ASSERT_STR_CONTAINS(res.error, "invalid declaration name 'my-app'");
	result_free(&res);
}

TEST(Preprocessor, long_lines_not_truncated)
{
	// A path-array line far beyond the old 1024-byte fgets buffer must be
	// processed whole instead of being split mid-string-literal.
	char input[16384];
	usize used = 0;
	used += (usize)snprintf(
		input + used, sizeof(input) - used, "Executable app = {\n    .sources = { "
	);
	for (int i = 0; i < 60; ++i) {
		used += (usize)snprintf(
			input + used, sizeof(input) - used, "\"very/long/path/segment%02d/file.c\", ", i
		);
	}
	ASSERT_TRUE(
		snprintf(
			input + used, sizeof(input) - used,
			"NULL },\n};\nProject LongLine = { .targets = { &app } };\n"
		) >= 0
	);

	char *out __attribute__((cleanup(tf_free_str))) = NULL;
	ASSERT_RESULT_OK(preprocess_str(input, &out));
	ASSERT_NOT_NULL(out);
	ASSERT_STR_CONTAINS(out, "segment59/file.c");
	ASSERT_STR_CONTAINS(out, "NULL }");
}

TEST(Preprocessor, simple_executable)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .sources = { \"src/main.c\" },\n"
		"};\n"
		"Project SimpleProj = {\n"
		"    .targets = { &app },\n"
		"};\n",
		".kind = T_EXECUTABLE,", ".name = \"app\",", "(const char*[]){ \"src/main.c\" , NULL }",
		".name = \"SimpleProj\",", "(void*[]){ &app , NULL }",
		"Project *nour_get_project(void) { return &SimpleProj; }"
	);
}

TEST(Preprocessor, executable_with_root)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .root = \"src/main.c\",\n"
		"    .sources = { \"src/utils.c\" },\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		".kind = T_EXECUTABLE,", ".name = \"app\",", ".root = \"src/main.c\",",
		"(const char*[]){ \"src/utils.c\" , NULL }"
	);
}

TEST(Preprocessor, library_and_deps)

{
	ASSERT_PREPROCESS(
		"Library math = {\n"
		"    .type = STATIC,\n"
		"    .sources = { \"src/math.c\" },\n"
		"};\n"
		"Executable app = {\n"
		"    .sources = { \"src/main.c\" },\n"
		"    .deps = { &math },\n"
		"};\n"
		"Project LibProj = {\n"
		"    .targets = { &math, &app },\n"
		"};\n",
		".kind = T_LIBRARY,", ".name = \"math\",", ".deps = (void*[]){ &math , NULL }",
		"(void*[]){ &math, &app , NULL }"
	);
}

TEST(Preprocessor, multiline_array)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .cflags = {\n"
		"        \"-Wall\",\n"
		"        \"-O2\"\n"
		"    },\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		"(const char*[]){\n", "NULL\n\t}"
	);
}

TEST(Preprocessor, empty_array)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .defines = {},\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		"(const char*[]){ NULL }"
	);
}

TEST(Preprocessor, trailing_comma)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .sources = { \"src/main.c\", },\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		"(const char*[]){ \"src/main.c\",", "NULL }"
	);
}

TEST(Preprocessor, comments)
{
	ASSERT_PREPROCESS(
		"// Comment above\n"
		"Executable app = {\n"
		"    // Comment inside struct\n"
		"    .sources = { \"main.c\" },\n"
		"};\n"
		"// Comment between declarations\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		"// Comment above\n", "// Comment inside struct\n", "// Comment between declarations\n"
	);
}

TEST(Preprocessor, string_with_braces)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .defines = { \"BRACE_STR=\\\"{hello}\\\"\" },\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		"BRACE_STR"
	);
}

TEST(Preprocessor, error_missing_project)
{
	ASSERT_PREPROCESS_ERR(
		"Executable app = {\n"
		"    .sources = { \"src/main.c\" },\n"
		"};\n"
	);
}

TEST(Preprocessor, error_unclosed_array)
{
	ASSERT_PREPROCESS_ERR(
		"Executable app = {\n"
		"    .sources = {\n"
		"        \"src/main.c\"\n"
		";\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n"
	);
}

TEST(Preprocessor, module_and_mods)
{
	ASSERT_PREPROCESS(
		"Module core = {\n"
		"    .sources = { \"core/a.c\" },\n"
		"    .includes = { \"core\" },\n"
		"};\n"
		"Executable app = {\n"
		"    .sources = { \"src/main.c\" },\n"
		"    .mods = { &core },\n"
		"};\n"
		"Project P = {\n"
		"    .targets = { &app },\n"
		"};\n",
		".name = \"core\",", ".mods = (Module*[]){ &core , NULL }"
	);
}

TEST(Preprocessor, include_multifile_nour)
{
	TMPDIR_AUTO(td);

	char sub_dir[512];
	snprintf(sub_dir, sizeof(sub_dir), "%s/core", td.path);
	mkdir(sub_dir, 0755);

	char mod_path[512];
	snprintf(mod_path, sizeof(mod_path), "%s/core/mod.nour", td.path);
	{
		FILE *mf = fopen(mod_path, "w");
		ASSERT_NOT_NULL(mf);
		defer(fclose, mf);
		fputs("Module core = {\n    .sources = { \"*.c\" },\n};\n", mf);
	}

	char main_path[512];
	snprintf(main_path, sizeof(main_path), "%s/project.nour", td.path);
	{
		FILE *pf = fopen(main_path, "w");
		ASSERT_NOT_NULL(pf);
		defer(fclose, pf);
		fputs(
			"#include \"core/mod.nour\"\n"
			"Executable app = {\n"
			"    .mods = { &core },\n"
			"};\n"
			"Project P = {\n"
			"    .targets = { &app },\n"
			"};\n",
			pf
		);
	}

	char out_path[512];
	snprintf(out_path, sizeof(out_path), "%s/out.c", td.path);

	Result res = preprocess(main_path, out_path);
	ASSERT_RESULT_OK(res);

	FILE *out_f = fopen(out_path, "r");
	ASSERT_NOT_NULL(out_f);
	defer(fclose, out_f);
	char buf[4096];
	usize n = fread(buf, 1, sizeof(buf) - 1, out_f);
	buf[n] = '\0';

	ASSERT_TRUE(strstr(buf, "// ─────────────────── core/mod.nour ───────────────────") != NULL);
	ASSERT_TRUE(strstr(buf, "// ─────────────────── project.nour ───────────────────") != NULL);
	ASSERT_TRUE(strstr(buf, ".name = \"core\"") != NULL);
	ASSERT_TRUE(strstr(buf, "\"core/*.c\"") != NULL);
	ASSERT_TRUE(strstr(buf, ".mods = (Module*[]){ &core , NULL }") != NULL);
	ASSERT_TRUE(strstr(buf, "Project *nour_get_project(void)") != NULL);
}

TEST(Preprocessor, include_nested_modules)
{
	TMPDIR_AUTO(td);

	char dir_core[512], dir_sub[512], dir_deep[512];
	snprintf(dir_core, sizeof(dir_core), "%s/core", td.path);
	snprintf(dir_sub, sizeof(dir_sub), "%s/core/sub", td.path);
	snprintf(dir_deep, sizeof(dir_deep), "%s/core/sub/deep", td.path);
	mkdir(dir_core, 0755);
	mkdir(dir_sub, 0755);
	mkdir(dir_deep, 0755);

	// 1. core/sub/deep/mod.nour
	char deep_mod_path[512];
	snprintf(deep_mod_path, sizeof(deep_mod_path), "%s/core/sub/deep/mod.nour", td.path);
	{
		FILE *df = fopen(deep_mod_path, "w");
		ASSERT_NOT_NULL(df);
		defer(fclose, df);
		fputs("Module deep = {\n    .sources = { \"*.c\" },\n    .includes = { \".\" },\n};\n", df);
	}

	// 2. core/sub/mod.nour (includes deep/mod.nour)
	char sub_mod_path[512];
	snprintf(sub_mod_path, sizeof(sub_mod_path), "%s/core/sub/mod.nour", td.path);
	{
		FILE *sf = fopen(sub_mod_path, "w");
		ASSERT_NOT_NULL(sf);
		defer(fclose, sf);
		fputs(
			"#include \"deep/mod.nour\"\n"
			"Module sub = {\n"
			"    .sources = { \"*.c\" },\n"
			"    .includes = { \".\" },\n"
			"    .mods = { &deep },\n"
			"};\n",
			sf
		);
	}

	// 3. core/mod.nour (includes sub/mod.nour)
	char core_mod_path[512];
	snprintf(core_mod_path, sizeof(core_mod_path), "%s/core/mod.nour", td.path);
	{
		FILE *cf = fopen(core_mod_path, "w");
		ASSERT_NOT_NULL(cf);
		defer(fclose, cf);
		fputs(
			"#include \"sub/mod.nour\"\n"
			"Module core = {\n"
			"    .sources = { \"*.c\" },\n"
			"    .includes = { \".\", \"../include\" },\n"
			"    .mods = { &sub },\n"
			"};\n",
			cf
		);
	}

	// 4. project.nour (includes core/mod.nour)
	char main_path[512];
	snprintf(main_path, sizeof(main_path), "%s/project.nour", td.path);
	{
		FILE *pf = fopen(main_path, "w");
		ASSERT_NOT_NULL(pf);
		defer(fclose, pf);
		fputs(
			"#include \"core/mod.nour\"\n"
			"Executable app = {\n"
			"    .sources = { \"src/main.c\" },\n"
			"    .mods = { &core },\n"
			"};\n"
			"Project NestedProj = {\n"
			"    .targets = { &app },\n"
			"};\n",
			pf
		);
	}

	char out_path[512];
	snprintf(out_path, sizeof(out_path), "%s/out.c", td.path);

	Result res = preprocess(main_path, out_path);
	ASSERT_RESULT_OK(res);

	FILE *out_f = fopen(out_path, "r");
	ASSERT_NOT_NULL(out_f);
	defer(fclose, out_f);
	char buf[8192];
	usize n = fread(buf, 1, sizeof(buf) - 1, out_f);
	buf[n] = '\0';

	// Check banners
	ASSERT_TRUE(
		strstr(buf, "// ─────────────────── core/sub/deep/mod.nour ───────────────────") != NULL
	);
	ASSERT_TRUE(
		strstr(buf, "// ─────────────────── core/sub/mod.nour ───────────────────") != NULL
	);
	ASSERT_TRUE(strstr(buf, "// ─────────────────── core/mod.nour ───────────────────") != NULL);
	ASSERT_TRUE(strstr(buf, "// ─────────────────── project.nour ───────────────────") != NULL);
	ASSERT_TRUE(strstr(buf, "// ─────────────────── Entrypoint ───────────────────") != NULL);

	// Check computed paths for deep
	ASSERT_TRUE(strstr(buf, "\"core/sub/deep/*.c\"") != NULL);
	ASSERT_TRUE(strstr(buf, "\"core/sub/deep\"") != NULL);

	// Check computed paths for sub
	ASSERT_TRUE(strstr(buf, "\"core/sub/*.c\"") != NULL);
	ASSERT_TRUE(strstr(buf, "\"core/sub\"") != NULL);

	// Check computed paths for core
	ASSERT_TRUE(strstr(buf, "\"core/*.c\"") != NULL);
	ASSERT_TRUE(strstr(buf, "\"core/../include\"") != NULL);

	// Check entrypoint function
	ASSERT_TRUE(strstr(buf, "Project *nour_get_project(void) { return &NestedProj; }") != NULL);
}
