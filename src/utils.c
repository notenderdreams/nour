#include "utils.h"
#include "types.h"
#include <stdio.h>

void print_project(const Project *proj)
{
	printf("\n[nour] %s v%s\n\n", proj->name, proj->version ? proj->version : "0.0.0");
	printf("Targets:\n");

	if (!proj->targets) {
		printf("  (none)\n");
		return;
	}

	usize count = 0;
	while (proj->targets[count]) {
		++count;
	}

	for (usize i = 0; i < count; ++i) {
		Target	   *target		   = (Target *)proj->targets[i];
		bool		is_last_target = (i == count - 1);
		const char *t_branch	   = is_last_target ? "  └── " : "  ├── ";
		const char *c_prefix	   = is_last_target ? "      " : "  │   ";

		printf("%s%s\n", t_branch, target->name);

		const char	*kind_str = "unknown";
		const char **sources  = NULL;

		if (target->kind == T_EXECUTABLE) {
			Executable *exe = (Executable *)target;
			kind_str		= "executable";
			sources			= exe->sources;
		} else if (target->kind == T_LIBRARY) {
			Library *lib = (Library *)target;
			switch (lib->type) {
			case SHARED:
				kind_str = "shared library";
				break;
			case STATIC:
				kind_str = "static library";
				break;
			case INTERFACE:
				kind_str = "interface library";
				break;
			}
			sources = lib->sources;
		}

		usize src_count = 0;
		if (sources) {
			while (sources[src_count]) {
				++src_count;
			}
		}

		if (src_count > 0) {
			printf("%s├── %s\n", c_prefix, kind_str);
			for (usize s = 0; s < src_count; ++s) {
				bool is_last_src = (s == src_count - 1);
				printf("%s%s %s\n", c_prefix, is_last_src ? "└──" : "├──", sources[s]);
			}
		} else {
			printf("%s└── %s\n", c_prefix, kind_str);
		}

		if (!is_last_target) {
			printf("  │\n");
		}
	}
}
