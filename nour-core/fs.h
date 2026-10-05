#pragma once

#include "types.h"

bool   fs_file_exists(const char *path);
bool   fs_dir_exists(const char *path);
Result fs_create_dir(const char *path);
Result fs_create_file(const char *path, const char *content);
Result fs_remove_dir(const char *path);
