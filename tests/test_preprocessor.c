#include "test.h"
#include "parser.h"

TEST(Preprocessor, simple_executable)
{
	ASSERT_PREPROCESS(
		"Executable app = {\n"
		"    .sources = { \"src/main.c\" },\n"
		"};\n"
		"Project SimpleProj = {\n"
		"    .targets = { &app },\n"
		"};\n",
		".kind = T_EXECUTABLE,",
		".name = \"app\",",
		"(const char*[]){ \"src/main.c\" , NULL }",
		".name = \"SimpleProj\",",
		"(void*[]){ &app , NULL }",
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
		".kind = T_EXECUTABLE,",
		".name = \"app\",",
		".root = \"src/main.c\",",
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
		".kind = T_LIBRARY,",
		".name = \"math\",",
		".deps = (void*[]){ &math , NULL }",
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
		"(const char*[]){\n",
		"NULL\n\t}"
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
		"(const char*[]){ \"src/main.c\",",
		"NULL }"
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
		"// Comment above\n",
		"// Comment inside struct\n",
		"// Comment between declarations\n"
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
