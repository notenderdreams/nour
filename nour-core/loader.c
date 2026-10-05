#include "loader.h"
#include "nour.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void *g_loader_handle = NULL;

typedef Project *(*NourGetProjectFn)(void);

Result compile_nour(const char *src, const char *dest)
{
	char cmd[PATH_MAX * 2 + 64];
	snprintf(cmd, sizeof(cmd), "cc -shared -fPIC \"%s\" -o \"%s\"", src, dest);

	i32 ret = system(cmd);
	if (ret) {
		return Err("Failed to compile '%s' into shared library", src);
	}
	return Ok(NULL);
}

Result load_project(const char *path)
{
	loader_close();
	g_loader_handle = dlopen(path, RTLD_NOW);
	if (!g_loader_handle) {
		return Err("dlopen failed: %s", dlerror());
	}

	NourGetProjectFn get_project = (NourGetProjectFn)dlsym(g_loader_handle, "nour_get_project");
	if (!get_project) {
		Result r = Err("dlsym failed: %s", dlerror());
		loader_close();
		return r;
	}

	Project *project = get_project();
	if (!project) {
		loader_close();
		return Err("nour_get_project returned NULL");
	}

	return Ok(project);
}

void loader_close(void)
{
	if (g_loader_handle) {
		dlclose(g_loader_handle);
		g_loader_handle = NULL;
	}
}
