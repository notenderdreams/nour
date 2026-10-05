cc      := "cc"
cflags  := "-O2 -Wall -Wextra -std=c11 -I. -Iinclude -Inour-core -Inour-cli -D_POSIX_C_SOURCE=200809L"
ldflags := "-ldl"

default: build

gen:
    xxd -i include/nour.h > nour-core/nour_header.h

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

