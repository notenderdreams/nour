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
