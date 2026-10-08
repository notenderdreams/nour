#pragma once
#include "types.h"

void ui_header(const char *version);

// Status Badges
PRINTF_FMT(2, 3) void ui_status(const char *verb, const char *fmt, ...);
void ui_created(const char *path);
void ui_running(const char *cmd);
PRINTF_FMT(1, 2) void ui_success(const char *fmt, ...);

// Error Diagnostics & Info
PRINTF_FMT(1, 2) void ui_error(const char *fmt, ...);
PRINTF_FMT(1, 2) void ui_info(const char *fmt, ...);

// Onboarding Guidance & Helpers
void ui_next_steps(const char *dir, const char *cmd_hint);
void ui_format_duration(char *buf, usize cap, f64 duration_ms);

// 54-column Tree Step
void ui_tree_step(bool is_last, const char *label, f64 duration_ms);
