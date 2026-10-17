#pragma once

#include "types.h"

Result compile_manifest(const char *src, const char *dest);

Result load_manifest(const char *path);

void unload_manifest(void);
