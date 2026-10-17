#include "loader.h"
#include "nour.h"
#include "arena.h"
#include "utils.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void *g_loader_handle = NULL;

typedef Project *(*NourGetProjectFn)(void);

Result compile_manifest(const char *src, const char *dest)
{
	arena_scratch(a, 512);
	const char *cmd =
		rnstr_sprintf(&a, "cc -shared -fPIC %s -o %s", sh_quote(&a, src), sh_quote(&a, dest));

	i32 ret = system(cmd);
	if (ret) {
		return Err(ERR_COMPILE_FAIL, src);
	}
	return Ok(NULL);
}

Result load_manifest(const char *path)
{
	unload_manifest();
	g_loader_handle = dlopen(path, RTLD_NOW);
	if (!g_loader_handle) {
		return Err(ERR_DLOPEN, dlerror());
	}

	NourGetProjectFn get_project = (NourGetProjectFn)dlsym(g_loader_handle, "nour_get_project");
	if (!get_project) {
		Result r = Err(ERR_DLSYM, dlerror());
		unload_manifest();
		return r;
	}

	Project *project = get_project();
	if (!project) {
		unload_manifest();
		return Err(ERR_NULL_PROJECT);
	}

	return Ok(project);
}

void unload_manifest(void)
{
	if (g_loader_handle) {
		dlclose(g_loader_handle);
		g_loader_handle = NULL;
	}
}
