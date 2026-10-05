#!/usr/bin/env bash
set -euo pipefail

# ANSI Colors
BOLD="\033[1m"
GREEN="\033[32m"
RED="\033[31m"
GRAY="\033[90m"
RESET="\033[0m"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NOUR_BIN="${REPO_ROOT}/bin/nour"

if [[ ! -x "${NOUR_BIN}" ]]; then
    echo -e "${RED}Error: ${NOUR_BIN} not found. Run 'just build' first.${RESET}"
    exit 1
fi

TEMP_DIR=$(mktemp -d /tmp/nour_cli_test_XXXXXX)
trap 'rm -rf "${TEMP_DIR}"' EXIT

PASSED=0
FAILED=0

test_assert() {
    local name="$1"
    local expected_code="$2"
    local cmd="$3"
    local stdout_pattern="${4:-}"
    local stderr_pattern="${5:-}"

    local out_file="${TEMP_DIR}/out.txt"
    local err_file="${TEMP_DIR}/err.txt"

    set +e
    eval "${cmd}" > "${out_file}" 2> "${err_file}"
    local actual_code=$?
    set -e

    local failed=0
    if [[ ${actual_code} -ne ${expected_code} ]]; then
        echo -e "  ${RED}✗ [${name}]${RESET} expected exit code ${expected_code}, got ${actual_code}"
        failed=1
    elif [[ -n "${stdout_pattern}" ]] && ! grep -q "${stdout_pattern}" "${out_file}"; then
        echo -e "  ${RED}✗ [${name}]${RESET} stdout missing pattern: '${stdout_pattern}'"
        failed=1
    elif [[ -n "${stderr_pattern}" ]] && ! grep -q "${stderr_pattern}" "${err_file}"; then
        echo -e "  ${RED}✗ [${name}]${RESET} stderr missing pattern: '${stderr_pattern}'"
        failed=1
    fi

    if [[ ${failed} -eq 0 ]]; then
        echo -e "  ${GREEN}✓${RESET} ${name}"
        PASSED=$((PASSED + 1))
    else
        FAILED=$((FAILED + 1))
    fi
}

echo -e "\n${BOLD}Running Nour CLI End-to-End Test Suite${RESET}\n"

# 1. Help & Version
test_assert "nour --help" 0 "'${NOUR_BIN}' --help" "USAGE"
test_assert "nour -h" 0 "'${NOUR_BIN}' -h" "USAGE"
test_assert "nour help" 0 "'${NOUR_BIN}' help" "USAGE"
test_assert "nour help build" 0 "'${NOUR_BIN}' help build" "build — Compile the project"
test_assert "nour --version" 0 "'${NOUR_BIN}' --version" "nour v0.1.0"
test_assert "nour -V" 0 "'${NOUR_BIN}' -V" "nour v0.1.0"
test_assert "nour -v" 0 "'${NOUR_BIN}' -v" "nour v0.1.0"

# 2. Subcommand Help
test_assert "nour build --help" 0 "'${NOUR_BIN}' build --help" "build — Compile the project"
test_assert "nour run --help" 0 "'${NOUR_BIN}' run --help" "run — Builds then runs"
test_assert "nour clean --help" 0 "'${NOUR_BIN}' clean --help" "clean — Remove build artifacts"
test_assert "nour new --help" 0 "'${NOUR_BIN}' new --help" "new — Create a new project directory"

# 3. Error handling
test_assert "unknown command" 1 "'${NOUR_BIN}' nonexistent_cmd" "" "error: unknown command 'nonexistent_cmd'"
test_assert "unknown flag" 1 "'${NOUR_BIN}' build --bad-flag" "" "error: unknown flag '--bad-flag'"
test_assert "build invalid target" 1 "'${NOUR_BIN}' build --target not_exists" "" "target 'not_exists' not found in project"
test_assert "run invalid target" 1 "'${NOUR_BIN}' run --target not_exists" "" "target 'not_exists' not found in project"
test_assert "new without name" 1 "'${NOUR_BIN}' new" "" "requires a project name"
test_assert "new invalid identifier" 1 "'${NOUR_BIN}' new 123-bad" "" "invalid project name"

# 4. Scaffolding with 'new'
NEW_DIR="${TEMP_DIR}/scaffold_test"
mkdir -p "${NEW_DIR}"
test_assert "new project scaffolding" 0 "cd '${NEW_DIR}' && '${NOUR_BIN}' new test_app" "Project 'test_app' created successfully"
test_assert "new project files exist" 0 "test -f '${NEW_DIR}/test_app/project.nour' && test -f '${NEW_DIR}/test_app/src/main.c' && test -f '${NEW_DIR}/test_app/.gitignore' && grep -q '\.nour/' '${NEW_DIR}/test_app/.gitignore'"
test_assert "new duplicate rejection" 1 "cd '${NEW_DIR}' && '${NOUR_BIN}' new test_app" "" "already exists"

# 5. Build, Run, Clean Workflow
test_assert "build new project" 0 "'${NOUR_BIN}' build '${NEW_DIR}/test_app'" "Finished build"
test_assert "build artifact exists" 0 "test -x '${NEW_DIR}/test_app/build/test_app' && test -f '${NEW_DIR}/test_app/.nour/libnour.so'"
test_assert "run project" 0 "'${NOUR_BIN}' run '${NEW_DIR}/test_app'" "Hello, test_app!"
test_assert "clean project" 0 "'${NOUR_BIN}' clean '${NEW_DIR}/test_app'" "Removed"
test_assert "clean removed build directory" 0 "! test -d '${NEW_DIR}/test_app/build' && ! test -d '${NEW_DIR}/test_app/.nour'"

# 6. Scaffolding with 'init'
INIT_DIR="${TEMP_DIR}/init_test"
mkdir -p "${INIT_DIR}"
test_assert "init current directory" 0 "cd '${INIT_DIR}' && '${NOUR_BIN}' init init_app" "Initialized Nour project"
test_assert "init files exist" 0 "test -f '${INIT_DIR}/project.nour' && test -f '${INIT_DIR}/src/main.c' && test -f '${INIT_DIR}/.gitignore'"
test_assert "init duplicate rejection" 1 "cd '${INIT_DIR}' && '${NOUR_BIN}' init" "" "already exists in current directory"

# Summary
echo -e "\n──────────────────────────────────────────────────────"
if [[ ${FAILED} -eq 0 ]]; then
    echo -e "${GREEN}✓ All ${PASSED} CLI tests passed!${RESET}\n"
    exit 0
else
    echo -e "${RED}✗ ${FAILED} failed, ${PASSED} passed.${RESET}\n"
    exit 1
fi
