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

clean:
    rm -rf bin build sandbox/build nour
    rm -f src/nour_header.h
