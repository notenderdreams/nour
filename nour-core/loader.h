#pragma once

#include "types.h"

Result compile_nour(const char *src, const char *dest);

Result load_project(const char *path);

void loader_close(void);
