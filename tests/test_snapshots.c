#include "test.h"
#include "parser.h"
#include "types.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef FIXTURES_DIR
#define FIXTURES_DIR "tests/fixtures"
#endif

static char *read_file_to_string(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	i64 size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size < 0) {
		fclose(f);
		return NULL;
	}

	char *buf = malloc(size + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	usize read_bytes = fread(buf, 1, size, f);
	buf[read_bytes]	 = '\0';
	fclose(f);
	return buf;
}

static i32 compare_strings(const void *a, const void *b)
{
	const char *const *sa = a;
	const char *const *sb = b;
	return strcmp(*sa, *sb);
}

static void first_diff(const char *a, const char *b, char *out, usize cap)
{
	i32 line = 1;
	while (*a && *a == *b) {
		if (*a == '\n')
			++line;
		++a;
		++b;
	}
	snprintf(out, cap, "differs from expected.c at line %d (bless: just test-bless)", line);
}

void run_snapshot_tests(const char *filter)
{
	const char *fixtures_dir = FIXTURES_DIR;
	DIR		   *dir			 = opendir(fixtures_dir);
	if (!dir) {
		SUITE_BEGIN("Snapshots");
		record_test_result(
			"fixtures_directory", 0, false, "fixtures directory not found", fixtures_dir, 0
		);
		SUITE_END();
		return;
	}

	char *names[MAX_CASES];
	i32	  name_count = 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		char subpath[512];
		snprintf(subpath, sizeof(subpath), "%s/%s", fixtures_dir, entry->d_name);

		struct stat st;
		if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode)) {
			if (name_count < MAX_CASES) {
				names[name_count++] = strdup(entry->d_name);
			}
		}
	}
	closedir(dir);

	qsort(names, name_count, sizeof(char *), compare_strings);

	const char *bless_env = getenv("NOUR_TEST_BLESS");
	bool		bless_mode =
		(bless_env && (strcmp(bless_env, "1") == 0 || strcmp(bless_env, "true") == 0));

	const char *tmp = getenv("TMPDIR");
	if (!tmp || !*tmp)
		tmp = "/tmp";

	SUITE_BEGIN("Snapshots");

	for (i32 i = 0; i < name_count; ++i) {
		const char *fixture_name = names[i];

		if (filter && !strstr("Snapshots", filter) && !strstr(fixture_name, filter)) {
			free(names[i]);
			continue;
		}

		char input_path[512], expected_path[512], actual_path[512];
		snprintf(input_path, sizeof(input_path), "%s/%s/input.nour", fixtures_dir, fixture_name);
		snprintf(
			expected_path, sizeof(expected_path), "%s/%s/expected.c", fixtures_dir, fixture_name
		);
		snprintf(
			actual_path, sizeof(actual_path), "%s/nour_snap_%s_%d_actual.c", tmp, fixture_name,
			getpid()
		);

		u64	 start_ns	  = tf_time_ns();
		bool passed		  = true;
		char err_buf[256] = { 0 };

		Result res = preprocess(input_path, actual_path);
		if (!res.ok) {
			passed = false;
			snprintf(
				err_buf, sizeof(err_buf), "Preprocess failed: %s",
				res.error ? res.error : "(unknown)"
			);
			result_free(&res);
		} else {
			char *actual_content   = read_file_to_string(actual_path);
			char *expected_content = read_file_to_string(expected_path);

			if (!actual_content) {
				passed = false;
				snprintf(err_buf, sizeof(err_buf), "Cannot read generated output file");
			} else if (bless_mode) {
				if (!expected_content || strcmp(actual_content, expected_content) != 0) {
					FILE *exp = fopen(expected_path, "wb");
					if (exp) {
						fputs(actual_content, exp);
						fclose(exp);
					}
				}
			} else {
				if (!expected_content) {
					passed = false;
					snprintf(err_buf, sizeof(err_buf), "Missing expected.c (bless: just test-bless)");
				} else if (strcmp(actual_content, expected_content) != 0) {
					passed = false;
					first_diff(actual_content, expected_content, err_buf, sizeof(err_buf));
				}
			}

			free(actual_content);
			free(expected_content);
			remove(actual_path);
		}

		f64 ms = (f64)(tf_time_ns() - start_ns) / 1000000.0;
		record_test_result(fixture_name, ms, passed, err_buf, expected_path, 0);
		free(names[i]);
	}

	SUITE_END();
}
