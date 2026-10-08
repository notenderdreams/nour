#include "test.h"
#include "str.h"
#include "utils.h"
#include <string.h>

TEST(StrView, creation_and_predicates)
{
	StrView lit = SV("hello");
	ASSERT_EQ(lit.len, 5);
	ASSERT_TRUE(sv_eqc(lit, "hello"));
	ASSERT_FALSE(sv_empty(lit));

	StrView empty = SV("");
	ASSERT_EQ(empty.len, 0);
	ASSERT_TRUE(sv_empty(empty));

	StrView from_c = sv_fromc("world");
	ASSERT_EQ(from_c.len, 5);
	ASSERT_TRUE(sv_eq(from_c, SV("world")));

	StrView from_null = sv_fromc(NULL);
	ASSERT_TRUE(sv_empty(from_null));

	StrView from_new = sv_new("abcdef", 3);
	ASSERT_EQ(from_new.len, 3);
	ASSERT_TRUE(sv_eqc(from_new, "abc"));
}

TEST(StrView, comparison_and_matching)
{
	ASSERT_TRUE(sv_starts(SV("foobar"), SV("foo")));
	ASSERT_FALSE(sv_starts(SV("foobar"), SV("bar")));
	ASSERT_FALSE(sv_starts(SV("foo"), SV("foobar")));

	ASSERT_TRUE(sv_ends(SV("foobar"), SV("bar")));
	ASSERT_FALSE(sv_ends(SV("foobar"), SV("foo")));
	ASSERT_FALSE(sv_ends(SV("bar"), SV("foobar")));

	// sv_contains
	ASSERT_TRUE(sv_contains(SV("hello world"), SV("world")));
	ASSERT_TRUE(sv_contains(SV("hello world"), SV("hello")));
	ASSERT_TRUE(sv_contains(SV("hello world"), SV("lo wo")));
	ASSERT_FALSE(sv_contains(SV("hello world"), SV("xyz")));
	ASSERT_TRUE(sv_containsc(SV("targets = { .deps }"), ".deps"));
	ASSERT_FALSE(sv_containsc(SV("targets = { .deps }"), ".flags"));

	// sv_cmp
	ASSERT_EQ(sv_cmp(SV("apple"), SV("apple")), 0);
	ASSERT_EQ(sv_cmp(SV("apple"), SV("banana")), -1);
	ASSERT_EQ(sv_cmp(SV("banana"), SV("apple")), 1);
	ASSERT_EQ(sv_cmp(SV("cat"), SV("catalog")), -1);
	ASSERT_EQ(sv_cmp(SV("catalog"), SV("cat")), 1);
	ASSERT_EQ(sv_cmp(SV(""), SV("")), 0);
	ASSERT_EQ(sv_cmp(SV(""), SV("a")), -1);
	ASSERT_EQ(sv_cmp(SV("a"), SV("")), 1);
}

TEST(StrView, slicing_and_trimming)
{
	// sv_trim
	ASSERT_TRUE(sv_eq(sv_trim(SV("  hello world  \t\n")), SV("hello world")));
	ASSERT_TRUE(sv_eq(sv_trim(SV("   ")), SV("")));
	ASSERT_TRUE(sv_eq(sv_trim(SV("")), SV("")));
	ASSERT_TRUE(sv_eq(sv_trim(SV("hello")), SV("hello")));

	// sv_sub
	StrView base = SV("0123456789");
	ASSERT_TRUE(sv_eq(sv_sub(base, 2, 4), SV("2345")));
	ASSERT_TRUE(sv_eq(sv_sub(base, 8, 10), SV("89")));
	ASSERT_TRUE(sv_eq(sv_sub(base, 20, 5), SV("")));

	// sv_split
	StrView left, right;
	bool	split = sv_split(SV("key=value"), '=', &left, &right);
	ASSERT_TRUE(split);
	ASSERT_TRUE(sv_eq(left, SV("key")));
	ASSERT_TRUE(sv_eq(right, SV("value")));

	split = sv_split(SV("no_delimiter"), '=', &left, &right);
	ASSERT_FALSE(split);
	ASSERT_TRUE(sv_eq(left, SV("no_delimiter")));
	ASSERT_TRUE(sv_empty(right));
}

TEST(StrView, to_cstr_conversion)
{
	StrView sv = SV("allocated_copy");
	char *cstr = sv_dup(sv);
	defer(free, cstr);
	ASSERT_NOT_NULL(cstr);
	ASSERT_STR_EQ(cstr, "allocated_copy");

	char *empty_cstr = sv_dup(SV(""));
	defer(free, empty_cstr);
	ASSERT_NOT_NULL(empty_cstr);
	ASSERT_STR_EQ(empty_cstr, "");
}

TEST(StrView, semver_parsing)
{
	i32 major = -1, minor = -1, patch = -1;

	parse_semver("1.2.3", &major, &minor, &patch);
	ASSERT_EQ(major, 1);
	ASSERT_EQ(minor, 2);
	ASSERT_EQ(patch, 3);

	parse_semver("v2.5.0", &major, &minor, &patch);
	ASSERT_EQ(major, 2);
	ASSERT_EQ(minor, 5);
	ASSERT_EQ(patch, 0);

	parse_semver("0.1.0-beta.2", &major, &minor, &patch);
	ASSERT_EQ(major, 0);
	ASSERT_EQ(minor, 1);
	ASSERT_EQ(patch, 0);

	parse_semver(NULL, &major, &minor, &patch);
	ASSERT_EQ(major, 0);
	ASSERT_EQ(minor, 0);
	ASSERT_EQ(patch, 0);
}
