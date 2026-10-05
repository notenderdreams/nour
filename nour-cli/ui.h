#pragma once
#include "types.h"

void ui_header(const char *version);

// Status Badges
void ui_status(const char *verb, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void ui_created(const char *path);
void ui_running(const char *cmd);
void ui_success(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Error Diagnostics & Info
void ui_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ui_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// 54-column Tree Step
void ui_tree_step(bool is_last, const char *label, f64 duration_ms);
