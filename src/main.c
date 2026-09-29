#include "builder.h"
#include "loader.h"
#include "nour.h"
#include "parser.h"
#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

i32 main(i32 argc, char *argv[])
{
	const char *project_dir = argc >= 2 ? argv[1] : ".";
	char		nour_path[512], c_path[512], so_path[512];

	snprintf(nour_path, sizeof(nour_path), "%s/project.nour", project_dir);
	snprintf(c_path, sizeof(c_path), "%s/build/project.nour.c", project_dir);
	snprintf(so_path, sizeof(so_path), "%s/build/libnour.so", project_dir);

	char mkdir_cmd[512];
	snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s/build\"", project_dir);
	system(mkdir_cmd);

	// Preprocess
	Result res = preprocess(nour_path, c_path);
	if (report(&res))
		return 1;
	printf("[preprocessed]: %s\n", c_path);

	// Compile
	res = compile_nour(c_path, so_path);
	if (report(&res))
		return 1;
	printf("[compiled]: %s\n", so_path);

	// Load
	res = load_project(so_path);
	if (report(&res))
		return 1;

	Project *proj = res.value;
	print_project(proj);

	// Build
	res = build_project(proj, project_dir);
	if (report(&res)) {
		loader_close();
		return 1;
	}

	const char *build_dir = project_build_dir(proj);
	for (usize i = 0; proj->targets && proj->targets[i]; ++i) {
		Target *target = (Target *)proj->targets[i];
		printf("\n[build]: Complete (%s/%s/%s)\n", project_dir, build_dir, target->name);
	}

	// Cleanup
	loader_close();
	return 0;
}
