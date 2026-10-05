#define TEST_MAIN
#include "test.h"

void run_snapshot_tests(const char *filter);

i32 main(i32 argc, char *argv[])
{
	const char *filter = (argc >= 2) ? argv[1] : NULL;

	tf_init_color();
	g_test_ctx.start_ns = tf_time_ns();
	tf_printf(ANSI_BOLD "Nour test suite" ANSI_RESET "\n\n");

	tf_run_registered(filter);
	run_snapshot_tests(filter);

	return test_summary();
}
