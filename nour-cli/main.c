#include "cmd.h"
#include "dispatch.h"
#include "types.h"

i32 main(i32 argc, char **argv)
{
	CLI cli = {
		.name		 = "nour",
		.version	 = NOUR_VERSION,
		.description = "A fast, zero-dependency C build system.",
		.commands	 = { &new_cmd, &init_cmd, &build_cmd, &run_cmd, &clean_cmd, CMD_END },
	};

	return dispatch(&cli, argc, argv);
}
