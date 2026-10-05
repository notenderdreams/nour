#include "fs.h"

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

	char cmd[PATH_MAX + 32];
	snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
	if (system(cmd) != 0) {
		return Err("failed to create directory '%s': %s", path, strerror(errno));
	}
	return Ok(NULL);
}

Result fs_create_file(const char *path, const char *content)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		return Err("failed to open file '%s': %s", path, strerror(errno));
	}
	if (content && fputs(content, f) == EOF) {
		fclose(f);
		return Err("failed to write content to '%s'", path);
	}
	fclose(f);
	return Ok(NULL);
}

Result fs_remove_dir(const char *path)
{
	if (!fs_dir_exists(path))
		return Ok(NULL);

	char cmd[PATH_MAX + 32];
	snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
	if (system(cmd) != 0) {
		return Err("failed to remove directory '%s': %s", path, strerror(errno));
	}
	return Ok(NULL);
}
