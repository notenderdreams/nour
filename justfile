cc      := "cc"
cflags  := "-O2 -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L"
ldflags := "-ldl"

default: build

gen:
    xxd -i include/nour.h > src/nour_header.h

build: gen
    mkdir -p bin
    {{cc}} {{cflags}} src/*.c -o bin/nour {{ldflags}}
    ln -sf bin/nour nour

run: build
	./bin/nour sandbox

self: build
	./bin/nour

test_flags := "-fsanitize=address,undefined -DFIXTURES_DIR='\"tests/fixtures\"'"

test: gen
	mkdir -p bin
	{{cc}} {{cflags}} {{test_flags}} -Isrc -Itests -D_DARWIN_C_SOURCE=1 src/parser.c src/loader.c src/utils.c src/builder.c tests/*.c -o bin/nour_test {{ldflags}}
	./bin/nour_test

test-bless: gen
	mkdir -p bin
	{{cc}} {{cflags}} {{test_flags}} -Isrc -Itests -D_DARWIN_C_SOURCE=1 src/parser.c src/loader.c src/utils.c src/builder.c tests/*.c -o bin/nour_test {{ldflags}}
	NOUR_TEST_BLESS=1 ./bin/nour_test


clean:
    rm -rf bin build sandbox/build nour tests/fixtures/*/actual.c
    rm -f src/nour_header.h

