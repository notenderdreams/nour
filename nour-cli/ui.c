#include "ui.h"
#include "theme.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void ui_header(const char *version)
{
	printf(
		"\n%s%snour%s %sv%s%s — %sC11 build system%s\n\n", th.bold, th.white, th.reset, th.slate,
		version, th.reset, th.dim, th.reset
	);
}

void ui_status(const char *verb, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("  %s%s%s%s ", th.bold, th.teal, verb, th.reset);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
}

void ui_created(const char *path)
{
	printf("  %s%sCreated%s %s\n", th.bold, th.emerald, th.reset, path);
}

void ui_running(const char *cmd)
{
	printf("  %s%sRunning%s %s%s%s\n", th.bold, th.lavender, th.reset, th.dim, cmd, th.reset);
}

void ui_success(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("  %s%s", th.emerald, th.bold);
	vprintf(fmt, args);
	printf("%s\n", th.reset);
	va_end(args);
}

void ui_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "\n  %serror:%s %s", th.crimson, th.reset, th.bold);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "%s\n\n", th.reset);
	va_end(args);
}

void ui_info(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("  %sinfo:%s ", th.teal, th.reset);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
}

void ui_tree_step(bool is_last, const char *label, f64 duration_ms)
{
	const char *branch = is_last ? th.corner : th.branch;

	char display_label[64];
	i32	 max_len = 38;
	if ((i32)strlen(label) > max_len) {
		snprintf(display_label, sizeof(display_label), "%.*s...", max_len - 3, label);
	} else {
		snprintf(display_label, sizeof(display_label), "%s", label);
	}

	char time_str[32];
	if (duration_ms >= 1000.0) {
		snprintf(time_str, sizeof(time_str), "%6.2f s", duration_ms / 1000.0);
	} else if (duration_ms >= 10.0) {
		snprintf(time_str, sizeof(time_str), "%6.1f ms", duration_ms);
	} else {
		snprintf(time_str, sizeof(time_str), "%6.2f ms", duration_ms);
	}

	printf(
		"  %s%s%s %-32s %s%10s%s\n", th.slate, branch, th.reset, display_label, th.slate, time_str,
		th.reset
	);
}
