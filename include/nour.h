#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct Target       Target;
typedef struct Executable   Executable;
typedef struct Library      Library;
typedef struct Project      Project;

typedef enum {
    T_EXECUTABLE,
    T_LIBRARY,
} TargetKind;

typedef enum {
    STATIC,
    SHARED,
    INTERFACE,
} LibraryType;

struct Target {
    TargetKind  kind;
    const char *name;
};

struct Executable {
    TargetKind   kind;
    const char  *name;
    const char **sources;
    const char **includes;
    const char **defines;
    const char **cflags;
    const char **ldflags;
    void       **deps;
};

struct Library {
    TargetKind   kind;
    const char  *name;
    LibraryType  type;
    const char **sources;
    const char **includes;
    const char **public_includes;
    const char **defines;
    const char **public_defines;
    const char **cflags;
    const char **ldflags;
    void       **deps;
};

struct Project {
    const char  *name;
    const char  *version;
    const char  *cc;
    const char  *build_dir;
    void       **targets;
};
