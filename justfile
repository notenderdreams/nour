cc      := "cc"
cflags  := "-O2 -Wall -Wextra -std=c11 -I. -I.nour -Iinclude -Inour-core -Inour-cli -D_POSIX_C_SOURCE=200809L"
ldflags := "-ldl"

default: build

gen:
    mkdir -p .nour/nour
    xxd -i include/nour.h > nour-core/nour_header.h
    test -f .nour/nour/config.h || printf '/* Bootstrap config header */\n#ifndef NOUR_CONFIG_H\n#define NOUR_CONFIG_H\n#define NOUR_PROJECT_NAME "nour"\n#define NOUR_PROJECT_VERSION "0.1.0"\n#define NOUR_VERSION NOUR_PROJECT_VERSION\n#define NOUR_PROJECT_BUILD_DIR "bin"\n#define NOUR_VERSION_MAJOR 0\n#define NOUR_VERSION_MINOR 1\n#define NOUR_VERSION_PATCH 0\n#define NOUR_TARGET_NAME "nour"\n#define NOUR_TARGET_KIND "executable"\n#define NOUR_BUILD_PROFILE "debug"\n#define NOUR_PROFILE_DEBUG 1\n#define NOUR_DEBUG 1\n#define NOUR_RELEASE 0\n#endif\n' > .nour/nour/config.h

build: gen
    mkdir -p bin
    {{cc}} {{cflags}} nour-core/*.c nour-cli/*.c -o bin/nour {{ldflags}}
    ln -sf bin/nour nour

run: build
	./bin/nour run sandbox

self: build
	./bin/nour build

test_flags := "-fsanitize=address,undefined -DFIXTURES_DIR='\"tests/fixtures\"'"

test: build
	mkdir -p bin
	{{cc}} {{cflags}} {{test_flags}} -Inour-core -Itests -D_DARWIN_C_SOURCE=1 nour-core/parser.c nour-core/loader.c nour-core/utils.c nour-core/builder.c nour-core/fs.c tests/*.c -o bin/nour_test {{ldflags}}
	./bin/nour_test

test-cli: build
	./tests/test_cli.sh

test-all: test test-cli

test-bless: build
	mkdir -p bin
	{{cc}} {{cflags}} {{test_flags}} -Inour-core -Itests -D_DARWIN_C_SOURCE=1 nour-core/parser.c nour-core/loader.c nour-core/utils.c nour-core/builder.c nour-core/fs.c tests/*.c -o bin/nour_test {{ldflags}}
	NOUR_TEST_BLESS=1 ./bin/nour_test

clean:
    rm -rf bin build sandbox/build nour tests/fixtures/*/actual.c .nour sandbox/.nour

fmt:
	find . -type f \( -name "*.c" -o -name "*.h" \) ! -name "nour_header.h" ! -path "*/fixtures/*" ! -path "*/build/*" ! -path "*/.git/*" -exec clang-format -i {} +

