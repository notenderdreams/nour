#include "test.h"
#include "loader.h"
#include "nour.h"
#include "parser.h"

TEST(Loader, end_to_end_project)
{
	TMPDIR_AUTO(td);
	LOADER_AUTO;

	char nour_path[512], c_path[512], so_path[512];
	snprintf(nour_path, sizeof(nour_path), "%s/project.nour", td.path);
	snprintf(c_path, sizeof(c_path), "%s/project.nour.c", td.path);
	snprintf(so_path, sizeof(so_path), "%s/libnour.so", td.path);

	const char *manifest_src =
		"Executable my_cli = {\n"
		"    .root     = \"src/main.c\",\n"
		"    .sources  = { \"src/main.c\", \"src/utils.c\" },\n"
		"    .includes = { \"include\" },\n"
		"    .cflags   = { \"-O3\", \"-DCLI_VERSION=1\" },\n"
		"};\n"
		"Project CliProject = {\n"
		"    .version   = \"2.5.0\",\n"
		"    .build_dir = \"out\",\n"
		"    .targets   = { &my_cli },\n"
		"};\n";

	FILE *f = fopen(nour_path, "w");
	ASSERT_NOT_NULL(f);
	fputs(manifest_src, f);
	fclose(f);

	// 1. Preprocess
	Result res = preprocess(nour_path, c_path);
	ASSERT_RESULT_OK(res);

	// 2. Compile to .so
	res = compile_nour(c_path, so_path);
	ASSERT_RESULT_OK(res);

	// 3. Load project
	res = load_project(so_path);
	ASSERT_RESULT_OK(res);
	ASSERT_NOT_NULL(res.value);

	Project *proj = (Project *)res.value;
	ASSERT_STR_EQ(proj->name, "CliProject");
	ASSERT_STR_EQ(proj->version, "2.5.0");
	ASSERT_STR_EQ(proj->build_dir, "out");
	ASSERT_NOT_NULL(proj->targets);

	// Target 0: Executable
	Target *t0 = (Target *)proj->targets[0];
	ASSERT_NOT_NULL(t0);
	ASSERT_EQ(t0->kind, T_EXECUTABLE);
	ASSERT_STR_EQ(t0->name, "my_cli");

	// Root & Sources
	Executable *exe = (Executable *)t0;
	ASSERT_STR_EQ(exe->root, "src/main.c");
	ASSERT_NOT_NULL(exe->sources);
	ASSERT_STR_EQ(exe->sources[0], "src/main.c");
	ASSERT_STR_EQ(exe->sources[1], "src/utils.c");
	ASSERT_NULL(exe->sources[2]);

	// Includes & cflags
	ASSERT_NOT_NULL(exe->includes);
	ASSERT_STR_EQ(exe->includes[0], "include");
	ASSERT_NULL(exe->includes[1]);

	ASSERT_NOT_NULL(exe->cflags);
	ASSERT_STR_EQ(exe->cflags[0], "-O3");
	ASSERT_STR_EQ(exe->cflags[1], "-DCLI_VERSION=1");
	ASSERT_NULL(exe->cflags[2]);

	ASSERT_NULL(proj->targets[1]);
}

TEST(Loader, fails_on_missing_so)
{
	Result res = load_project("/tmp/nour_does_not_exist_xyz123.so");
	ASSERT_RESULT_ERR(res);
}

TEST(Loader, fails_on_invalid_c_compilation)
{
	TMPDIR_AUTO(td);

	char bad_c[512], out_so[512];
	snprintf(bad_c, sizeof(bad_c), "%s/bad.c", td.path);
	snprintf(out_so, sizeof(out_so), "%s/bad.so", td.path);

	FILE *f = fopen(bad_c, "w");
	ASSERT_NOT_NULL(f);
	fprintf(f, "int this is total gibberish syntax error;\n");
	fclose(f);

	Result res;
	TEST_SILENT(res = compile_nour(bad_c, out_so));
	ASSERT_RESULT_ERR(res);
}

static Result helper_unwrap_ok(void)
{
	static i32 val = 99;
	Result	   r   = Ok(&val);
	i32		  *p   = Unwrap(r);
	if (*p != 99)
		return Err("mismatch");
	return Ok(p);
}

static Result helper_unwrap_err(void)
{
	Result r = Err("simulated failure");
	i32	  *p = Unwrap(r);
	(void)p;
	return Ok(NULL);
}

TEST(Loader, unwrap_macro)
{
	Result r1 = helper_unwrap_ok();
	ASSERT_RESULT_OK(r1);
	ASSERT_EQ(*(i32 *)r1.value, 99);

	ASSERT_RESULT_ERR_CONTAINS(helper_unwrap_err(), "simulated failure");
}
