#include "test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
	i32	 exit_code;
	char stdout_str[8192];
	char stderr_str[8192];
} CliOutput;

static bool get_nour_bin(char *buf, usize cap)
{
	(void)cap;
	if (realpath("bin/nour", buf))
		return true;
	if (realpath("./nour", buf))
		return true;
	if (realpath("../bin/nour", buf))
		return true;
	return false;
}

static bool run_cli(const char *cwd, const char *args, CliOutput *out)
{
	char nour_bin[PATH_MAX];
	if (!get_nour_bin(nour_bin, sizeof(nour_bin))) {
		return false;
	}

	char out_file[PATH_MAX], err_file[PATH_MAX];
	snprintf(out_file, sizeof(out_file), "/tmp/nour_out_%d.txt", getpid());
	snprintf(err_file, sizeof(err_file), "/tmp/nour_err_%d.txt", getpid());

	char cmd[4096];
	if (cwd && cwd[0]) {
		snprintf(
			cmd, sizeof(cmd), "cd \"%s\" && \"%s\" %s > \"%s\" 2> \"%s\"", cwd, nour_bin,
			args ? args : "", out_file, err_file
		);
	} else {
		snprintf(
			cmd, sizeof(cmd), "\"%s\" %s > \"%s\" 2> \"%s\"", nour_bin, args ? args : "", out_file,
			err_file
		);
	}

	i32 status = system(cmd);
	if (WIFEXITED(status)) {
		out->exit_code = WEXITSTATUS(status);
	} else {
		out->exit_code = -1;
	}

	out->stdout_str[0] = '\0';
	out->stderr_str[0] = '\0';

	FILE *f = fopen(out_file, "r");
	if (f) {
		usize n			   = fread(out->stdout_str, 1, sizeof(out->stdout_str) - 1, f);
		out->stdout_str[n] = '\0';
		fclose(f);
	}
	unlink(out_file);

	f = fopen(err_file, "r");
	if (f) {
		usize n			   = fread(out->stderr_str, 1, sizeof(out->stderr_str) - 1, f);
		out->stderr_str[n] = '\0';
		fclose(f);
	}
	unlink(err_file);

	return true;
}

TEST(CLI, help_flag)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "--help", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "nour v0.1.0");
	ASSERT_STR_CONTAINS(out.stdout_str, "USAGE");
	ASSERT_STR_CONTAINS(out.stdout_str, "COMMANDS");
	ASSERT_STR_CONTAINS(out.stdout_str, "new");
	ASSERT_STR_CONTAINS(out.stdout_str, "build");
	ASSERT_STR_CONTAINS(out.stdout_str, "run");
	ASSERT_STR_CONTAINS(out.stdout_str, "clean");

	CliOutput out_short;
	ASSERT_TRUE(run_cli(NULL, "-h", &out_short));
	ASSERT_EQ(out_short.exit_code, 0);
	ASSERT_STR_CONTAINS(out_short.stdout_str, "USAGE");
}

TEST(CLI, version_flag)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "--version", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "nour v0.1.0");

	CliOutput out_short;
	ASSERT_TRUE(run_cli(NULL, "-V", &out_short));
	ASSERT_EQ(out_short.exit_code, 0);
	ASSERT_STR_CONTAINS(out_short.stdout_str, "nour v0.1.0");
}

TEST(CLI, command_help)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "build --help", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "build — Compile the project");
	ASSERT_STR_CONTAINS(out.stdout_str, "--dir");
	ASSERT_STR_CONTAINS(out.stdout_str, "--target");

	ASSERT_TRUE(run_cli(NULL, "run --help", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "run — Builds then runs");

	ASSERT_TRUE(run_cli(NULL, "clean --help", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "clean — Remove build artifacts");
}

TEST(CLI, unknown_command)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "not_a_real_command", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "error: unknown command 'not_a_real_command'");
	ASSERT_STR_CONTAINS(out.stdout_str, "info: Run 'nour --help'");
}

TEST(CLI, unknown_flag)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "build --nonexistent-flag", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "error: unknown flag '--nonexistent-flag'");
}

TEST(CLI, new_requires_name)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "new", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "error: 'new' requires a project name");
}

TEST(CLI, new_rejects_invalid_identifier)
{
	TMPDIR_AUTO(td);
	CliOutput out;
	ASSERT_TRUE(run_cli(td.path, "new 123-invalid", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "invalid project name '123-invalid'");
}

TEST(CLI, new_scaffolds_project)
{
	TMPDIR_AUTO(td);
	CliOutput out;
	ASSERT_TRUE(run_cli(td.path, "new my_sample_app", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "Project 'my_sample_app' created successfully!");

	char manifest_path[512], main_path[512], gitignore_path[512];
	snprintf(manifest_path, sizeof(manifest_path), "%s/my_sample_app/project.nour", td.path);
	snprintf(main_path, sizeof(main_path), "%s/my_sample_app/src/main.c", td.path);
	snprintf(gitignore_path, sizeof(gitignore_path), "%s/my_sample_app/.gitignore", td.path);

	ASSERT_TRUE(access(manifest_path, F_OK) == 0);
	ASSERT_TRUE(access(main_path, F_OK) == 0);
	ASSERT_TRUE(access(gitignore_path, F_OK) == 0);

	// Running new with the same name again should fail with an error
	CliOutput out_duplicate;
	ASSERT_TRUE(run_cli(td.path, "new my_sample_app", &out_duplicate));
	ASSERT_EQ(out_duplicate.exit_code, 1);
	ASSERT_STR_CONTAINS(out_duplicate.stderr_str, "already exists");
}

TEST(CLI, init_scaffolds_current_directory)
{
	TMPDIR_AUTO(td);
	CliOutput out;
	ASSERT_TRUE(run_cli(td.path, "init my_curr_project", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "Initialized Nour project");

	char manifest_path[512], main_path[512], gitignore_path[512];
	snprintf(manifest_path, sizeof(manifest_path), "%s/project.nour", td.path);
	snprintf(main_path, sizeof(main_path), "%s/src/main.c", td.path);
	snprintf(gitignore_path, sizeof(gitignore_path), "%s/.gitignore", td.path);

	ASSERT_TRUE(access(manifest_path, F_OK) == 0);
	ASSERT_TRUE(access(main_path, F_OK) == 0);
	ASSERT_TRUE(access(gitignore_path, F_OK) == 0);

	// Running init again in the same directory should fail
	CliOutput out_duplicate;
	ASSERT_TRUE(run_cli(td.path, "init", &out_duplicate));
	ASSERT_EQ(out_duplicate.exit_code, 1);
	ASSERT_STR_CONTAINS(out_duplicate.stderr_str, "already exists in current directory");
}

TEST(CLI, build_run_and_clean_workflow)
{
	TMPDIR_AUTO(td);
	CliOutput out;

	// 1. Scaffold
	ASSERT_TRUE(run_cli(td.path, "new calc_app", &out));
	ASSERT_EQ(out.exit_code, 0);

	// 2. Build
	char project_dir[512];
	snprintf(project_dir, sizeof(project_dir), "%s/calc_app", td.path);
	ASSERT_TRUE(run_cli(td.path, "build calc_app", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "Building calc_app");
	ASSERT_STR_CONTAINS(out.stdout_str, "Finished build");

	char bin_file[512];
	snprintf(bin_file, sizeof(bin_file), "%s/calc_app/build/calc_app", td.path);
	ASSERT_TRUE(access(bin_file, X_OK) == 0);

	char so_file[512];
	snprintf(so_file, sizeof(so_file), "%s/calc_app/.nour/libnour.so", td.path);
	ASSERT_TRUE(access(so_file, F_OK) == 0);

	// 3. Run
	ASSERT_TRUE(run_cli(td.path, "run calc_app", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "Running");
	ASSERT_STR_CONTAINS(out.stdout_str, "Hello, calc_app!");

	// 4. Clean
	ASSERT_TRUE(run_cli(td.path, "clean calc_app", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "Removed");

	char build_dir[512];
	snprintf(build_dir, sizeof(build_dir), "%s/calc_app/build", td.path);
	ASSERT_TRUE(access(build_dir, F_OK) != 0);

	char dot_nour_dir[512];
	snprintf(dot_nour_dir, sizeof(dot_nour_dir), "%s/calc_app/.nour", td.path);
	ASSERT_TRUE(access(dot_nour_dir, F_OK) != 0);
}

TEST(CLI, run_forwards_arguments)
{
	TMPDIR_AUTO(td);
	CliOutput out;

	ASSERT_TRUE(run_cli(td.path, "new arg_tester", &out));
	ASSERT_EQ(out.exit_code, 0);

	// Overwrite main.c with a program that prints its arguments
	char main_file[512];
	snprintf(main_file, sizeof(main_file), "%s/arg_tester/src/main.c", td.path);
	FILE *f = fopen(main_file, "w");
	ASSERT_TRUE(f != NULL);
	fputs(
		"#include <stdio.h>\n"
		"int main(int argc, char *argv[]) {\n"
		"    for (int i = 1; i < argc; ++i) printf(\"ARG[%d]: %s\\n\", i, argv[i]);\n"
		"    return 0;\n"
		"}\n",
		f
	);
	fclose(f);

	// Run with forwarded arguments after --
	ASSERT_TRUE(run_cli(td.path, "run arg_tester -- apple banana \"cherry pie\"", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "ARG[1]: apple");
	ASSERT_STR_CONTAINS(out.stdout_str, "ARG[2]: banana");
	ASSERT_STR_CONTAINS(out.stdout_str, "ARG[3]: cherry pie");
}

TEST(CLI, target_validation)
{
	TMPDIR_AUTO(td);
	CliOutput out;

	ASSERT_TRUE(run_cli(td.path, "new target_app", &out));
	ASSERT_EQ(out.exit_code, 0);

	// Attempt to build nonexistent target
	ASSERT_TRUE(run_cli(td.path, "build target_app --target nonexistent", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "target 'nonexistent' not found in project");

	// Attempt to run nonexistent target
	ASSERT_TRUE(run_cli(td.path, "run target_app --target nonexistent", &out));
	ASSERT_EQ(out.exit_code, 1);
	ASSERT_STR_CONTAINS(out.stderr_str, "target 'nonexistent' not found in project");
}

TEST(CLI, run_forwards_existing_directory_name)
{
	TMPDIR_AUTO(td);
	CliOutput out;

	ASSERT_TRUE(run_cli(td.path, "new dir_arg_app", &out));
	ASSERT_EQ(out.exit_code, 0);

	char project_dir[PATH_MAX];
	snprintf(project_dir, sizeof(project_dir), "%s/dir_arg_app", td.path);

	// Create a subfolder inside the project named 'dummy_dir'
	char dummy_dir[PATH_MAX];
	snprintf(dummy_dir, sizeof(dummy_dir), "%s/dir_arg_app/dummy_dir", td.path);
	mkdir(dummy_dir, 0755);

	// Overwrite main.c to print argument
	char main_file[PATH_MAX];
	snprintf(main_file, sizeof(main_file), "%s/dir_arg_app/src/main.c", td.path);
	FILE *f = fopen(main_file, "w");
	ASSERT_TRUE(f != NULL);
	fputs(
		"#include <stdio.h>\n"
		"int main(int argc, char *argv[]) {\n"
		"    if (argc > 1) printf(\"ARG: %s\\n\", argv[1]);\n"
		"    return 0;\n"
		"}\n",
		f
	);
	fclose(f);

	// Run from inside dir_arg_app forwarding 'dummy_dir'
	ASSERT_TRUE(run_cli(project_dir, "run -- dummy_dir", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "ARG: dummy_dir");
}

TEST(CLI, clean_manifest_custom_build_dir)
{
	TMPDIR_AUTO(td);
	CliOutput out;

	ASSERT_TRUE(run_cli(td.path, "new custom_clean_app", &out));
	ASSERT_EQ(out.exit_code, 0);

	// Change build_dir in project.nour to "custom_bin"
	char manifest[PATH_MAX];
	snprintf(manifest, sizeof(manifest), "%s/custom_clean_app/project.nour", td.path);
	FILE *f = fopen(manifest, "w");
	ASSERT_TRUE(f != NULL);
	fputs(
		"Executable custom_clean_app = {\n"
		"    .root = \"src/main.c\",\n"
		"    .sources = { \"src/*.c\" },\n"
		"};\n\n"
		"Project custom_clean_app_project = {\n"
		"    .version = \"0.1.0\",\n"
		"    .build_dir = \"custom_bin\",\n"
		"    .targets = { &custom_clean_app },\n"
		"};\n",
		f
	);
	fclose(f);

	// Build project
	ASSERT_TRUE(run_cli(td.path, "build custom_clean_app", &out));
	ASSERT_EQ(out.exit_code, 0);

	char custom_bin[PATH_MAX];
	snprintf(custom_bin, sizeof(custom_bin), "%s/custom_clean_app/custom_bin", td.path);
	ASSERT_TRUE(access(custom_bin, F_OK) == 0);

	// Clean project and verify custom_bin is cleaned
	ASSERT_TRUE(run_cli(td.path, "clean custom_clean_app", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_TRUE(access(custom_bin, F_OK) != 0);
}

TEST(CLI, help_subcommand)
{
	CliOutput out;
	ASSERT_TRUE(run_cli(NULL, "help", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "USAGE");

	ASSERT_TRUE(run_cli(NULL, "help build", &out));
	ASSERT_EQ(out.exit_code, 0);
	ASSERT_STR_CONTAINS(out.stdout_str, "build — Compile the project");
}
