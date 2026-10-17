#include "arena.h"
#include "test.h"
#include "utils.h"
#include <string.h>

TEST(Utils, sh_quote_plain)
{
	arena_scratch(a, 256);
	ASSERT_STR_EQ(sh_quote(&a, "hello"), "'hello'");
	ASSERT_STR_EQ(sh_quote(&a, "a/b c"), "'a/b c'");
	// '$' and friends must never be expanded by the shell
	ASSERT_STR_EQ(sh_quote(&a, "$(touch /tmp/pwned)"), "'$(touch /tmp/pwned)'");
}

TEST(Utils, sh_quote_embedded_quote)
{
	arena_scratch(a, 256);
	ASSERT_STR_EQ(sh_quote(&a, "it's"), "'it'\\''s'");
}

TEST(Utils, sh_quote_null_and_empty)
{
	arena_scratch(a, 256);
	ASSERT_STR_EQ(sh_quote(&a, NULL), "''");
	ASSERT_STR_EQ(sh_quote(&a, ""), "''");
}

TEST(Utils, project_build_dir_default)
{
	Project p = {0};
	ASSERT_STR_EQ(project_build_dir(&p), "build");
}
