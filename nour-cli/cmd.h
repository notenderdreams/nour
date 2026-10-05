#pragma once
#include "dispatch.h"

Result cmd_new(Context *ctx);
Result cmd_init(Context *ctx);
Result cmd_build(Context *ctx);
Result cmd_run(Context *ctx);
Result cmd_clean(Context *ctx);

extern Command new_cmd;
extern Command init_cmd;
extern Command build_cmd;
extern Command run_cmd;
extern Command clean_cmd;
