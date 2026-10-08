#include "fs.h"
#include "arena.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool fs_file_exists(const char *path)
{
	struct stat st;
	return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

bool fs_dir_exists(const char *path)
{
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

Result fs_create_dir(const char *path)
{
	if (fs_dir_exists(path))
		return Ok(NULL);

	arena_scratch(a, 256);
	const char *cmd = rnstr_sprintf(&a, "mkdir -p \"%s\"", path);
	i32 ret = system(cmd);

	if (ret != 0) {
		return Err("failed to create directory '%s'", path);
	}
	return Ok(NULL);
}

Result fs_create_file(const char *path, const char *content)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		return Err("failed to open file '%s': %s", path, strerror(errno));
	}
	defer(fclose, f);

	if (content && fputs(content, f) == EOF) {
		return Err("failed to write content to '%s'", path);
	}
	return Ok(NULL);
}

Result fs_remove_dir(const char *path)
{
	if (!fs_dir_exists(path))
		return Ok(NULL);

	arena_scratch(a, 256);
	const char *cmd = rnstr_sprintf(&a, "rm -rf \"%s\"", path);
	i32 ret = system(cmd);

	if (ret != 0) {
		return Err("failed to remove directory '%s'", path);
	}
	return Ok(NULL);
}
