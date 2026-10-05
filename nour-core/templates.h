#pragma once

static const char MANIFEST_TEMPLATE[] =
	"Executable %s = {\n"
	"    .root     = \"src/main.c\",\n"
	"    .sources  = { \"src/*.c\" },\n"
	"    .includes = { \"include\" },\n"
	"    .cflags   = { \"-O2\", \"-Wall\" },\n"
	"};\n\n"
	"Project %s_project = {\n"
	"    .version   = \"0.1.0\",\n"
	"    .build_dir = \"build\",\n"
	"    .targets   = { &%s },\n"
	"};\n";

static const char MAIN_TEMPLATE[] =
	"#include <stdio.h>\n\n"
	"int main(void)\n"
	"{\n"
	"    printf(\"Hello, %s!\\n\");\n"
	"    return 0;\n"
	"}\n";

static const char GITIGNORE_TEMPLATE[] =
	".nour/\n"
	"build/\n"
	"bin/\n"
	"*.so\n"
	"*.o\n";

static const char CONFIG_HEADER_TEMPLATE[] =
	"/* Generated automatically by Nour. Do not edit. */\n"
	"#ifndef NOUR_CONFIG_H\n"
	"#define NOUR_CONFIG_H\n\n"
	"#define NOUR_PROJECT_NAME       \"%s\"\n"
	"#define NOUR_PROJECT_VERSION    \"%s\"\n"
	"#define NOUR_VERSION            NOUR_PROJECT_VERSION\n"
	"#define NOUR_PROJECT_BUILD_DIR  \"%s\"\n\n"
	"/* Semantic version breakdown */\n"
	"#define NOUR_VERSION_MAJOR      %d\n"
	"#define NOUR_VERSION_MINOR      %d\n"
	"#define NOUR_VERSION_PATCH      %d\n\n"
	"/* Target information */\n"
	"#define NOUR_TARGET_NAME        \"%s\"\n"
	"#define NOUR_TARGET_KIND        \"%s\"\n\n"
	"/* Build Profile */\n"
	"#define NOUR_BUILD_PROFILE      \"%s\"\n"
	"#define NOUR_PROFILE_%s 1\n\n"
	"/* Mode Tiers */\n"
	"#define NOUR_DEBUG              %d\n"
	"#define NOUR_RELEASE            %d\n\n"
	"/* Convenient short aliases (defined only if not previously defined) */\n"
	"#ifndef APP_NAME\n"
	"#  define APP_NAME NOUR_PROJECT_NAME\n"
	"#endif\n\n"
	"#ifndef APP_VERSION\n"
	"#  define APP_VERSION NOUR_PROJECT_VERSION\n"
	"#endif\n\n"
	"#endif /* NOUR_CONFIG_H */\n";
