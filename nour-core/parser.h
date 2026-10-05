#pragma once

#include "types.h"
#include <stdio.h>

Result preprocess_stream(FILE *in, FILE *out, const char *source_name);
Result preprocess_str(const char *input_str, char **output_str);
Result preprocess(const char *input_path, const char *output_path);
