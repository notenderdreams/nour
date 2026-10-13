#include "test.h"
#include "error.h"
#include <string.h>

TEST(Error, array_and_macro_constants)
{
	ASSERT_TRUE(messages[ERR_OK] != NULL);
	ASSERT_STR_EQ(messages[ERR_OK], "success");
	ASSERT_STR_EQ(messages[ERR_TARGET_EXIT], "target '%s' exited with code %d");
	ASSERT_STR_EQ(messages[ERR_TOO_MANY_ARGS], "too many arguments");
	ASSERT_STR_EQ(error_messages[ERR_FILE_NOT_FOUND], "could not find '%s'");
	ASSERT_STR_EQ(messages[ERR_UNKNOWN], "something went wrong: %s");
}

TEST(Error, construct_with_integers)
{
	Result r1 = Err(ERR_TARGET_EXIT, "my_app", 1);
	ASSERT_FALSE(r1.ok);
	ASSERT_TRUE(r1.error != NULL);
	ASSERT_STR_EQ(r1.error, "target 'my_app' exited with code 1");
	result_free(&r1);

	Result r2 = Err(ERR_DEPTH_LIMIT, 16, "config.nour");
	ASSERT_FALSE(r2.ok);
	ASSERT_TRUE(r2.error != NULL);
	ASSERT_STR_EQ(r2.error, "Include depth limit exceeded (max 16) while processing 'config.nour'");
	result_free(&r2);

	Result r3 = Err(ERR_INDEX_BOUNDS, 10, 5);
	ASSERT_FALSE(r3.ok);
	ASSERT_TRUE(r3.error != NULL);
	ASSERT_STR_EQ(r3.error, "index 10 out of bounds (max 5)");
	result_free(&r3);
}

TEST(Error, construct_with_strings_and_chars)
{
	Result r1 = Err(ERR_DIR_CREATE, "build/out");
	ASSERT_FALSE(r1.ok);
	ASSERT_TRUE(r1.error != NULL);
	ASSERT_STR_EQ(r1.error, "failed to create directory 'build/out'");
	result_free(&r1);

	Result r2 = Err(ERR_UNKNOWN_SHORT_FLAG, 'v');
	ASSERT_FALSE(r2.ok);
	ASSERT_TRUE(r2.error != NULL);
	ASSERT_STR_EQ(r2.error, "unknown flag '-v'");
	result_free(&r2);

	Result r3 = Err(ERR_TOO_MANY_ARGS);
	ASSERT_FALSE(r3.ok);
	ASSERT_TRUE(r3.error != NULL);
	ASSERT_STR_EQ(r3.error, "too many arguments");
	result_free(&r3);

	Result r4 = Err(ERR_UNKNOWN, "something unexpected");
	ASSERT_FALSE(r4.ok);
	ASSERT_TRUE(r4.error != NULL);
	ASSERT_STR_EQ(r4.error, "something went wrong: something unexpected");
	result_free(&r4);
}

TEST(Error, construct_raw_format_compatibility)
{
	Result r1 = Err("custom error with number: %d", 42);
	ASSERT_FALSE(r1.ok);
	ASSERT_TRUE(r1.error != NULL);
	ASSERT_STR_EQ(r1.error, "custom error with number: 42");
	result_free(&r1);

	Result r2 = err("lowercase err macro: %s", "ok");
	ASSERT_FALSE(r2.ok);
	ASSERT_TRUE(r2.error != NULL);
	ASSERT_STR_EQ(r2.error, "lowercase err macro: ok");
	result_free(&r2);
}

TEST(Error, unknown_or_out_of_bounds_code)
{
	Result r = Err(9999);
	ASSERT_FALSE(r.ok);
	ASSERT_TRUE(r.error != NULL);
	ASSERT_STR_EQ(r.error, "something went wrong: unknown error occurred (9999)");
	result_free(&r);

	Result r_neg = Err(-5);
	ASSERT_FALSE(r_neg.ok);
	ASSERT_TRUE(r_neg.error != NULL);
	ASSERT_STR_EQ(r_neg.error, "something went wrong: unknown error occurred (-5)");
	result_free(&r_neg);
}
