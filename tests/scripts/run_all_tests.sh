#!/bin/bash
# Run all ThunderOS tests.
#
# Builds the kernel ONCE (ENABLE_TESTS=1 TEST_MODE=1), then runs each test
# suite with --skip-build to avoid redundant rebuilds.
#
# Usage: ./run_all_tests.sh [--quick]
#   --quick  Boot test only (fast CI sanity check; rebuilds without test mode)

export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

source "${SCRIPT_DIR}/test_helpers.sh"

SUITES_FAILED=0
SUITES_TOTAL=0

run_suite() {
    local name="$1" script="$2"; shift 2
    SUITES_TOTAL=$((SUITES_TOTAL + 1))
    printf "\n"
    if bash "${SCRIPT_DIR}/${script}" "$@"; then
        return 0
    else
        SUITES_FAILED=$((SUITES_FAILED + 1))
        return 1
    fi
}

# ── Quick mode: boot test only ───────────────────────────────────────────────

if [ "${1:-}" = "--quick" ]; then
    run_suite "Boot" "test_boot.sh" || true
    printf "\n[${_B}==========${_N}] %d suite(s) ran.\n" "$SUITES_TOTAL"
    [ $SUITES_FAILED -eq 0 ] \
        && printf "[${_G}  PASSED  ${_N}] All %d suite(s).\n" "$SUITES_TOTAL" \
        && exit 0
    printf "[${_R}  FAILED  ${_N}] %d of %d suite(s) failed.\n" "$SUITES_FAILED" "$SUITES_TOTAL"
    exit 1
fi

# ── Full suite: build once, share across suites ──────────────────────────────

printf "[${_B}==========${_N}] ThunderOS Complete Test Suite\n"
printf "[ SETUP    ] Building kernel (ENABLE_TESTS=1 TEST_MODE=1)..."
cd "${ROOT_DIR}"
if make clean >/dev/null 2>&1 && make ENABLE_TESTS=1 TEST_MODE=1 >/dev/null 2>&1; then
    printf " OK\n"
else
    printf " FAILED\n"
    printf "[${_R}  ERROR   ${_N}] Kernel build failed. Run 'make ENABLE_TESTS=1 TEST_MODE=1' for details.\n" >&2
    exit 1
fi
printf "[ SETUP    ] Building userland..."
if make userland >/dev/null 2>&1; then
    printf " OK\n"
else
    printf " skipped\n"
fi

run_suite "Kernel Functionality" "test_kernel.sh" --skip-build || true
run_suite "Integration"          "test_integration.sh" --skip-build || true

printf "\n[${_B}==========${_N}] %d suite(s) ran.\n" "$SUITES_TOTAL"
if [ $SUITES_FAILED -eq 0 ]; then
    printf "[${_G}  PASSED  ${_N}] All %d suite(s).\n" "$SUITES_TOTAL"
    exit 0
else
    printf "[${_R}  FAILED  ${_N}] %d of %d suite(s) failed.\n" "$SUITES_FAILED" "$SUITES_TOTAL"
    exit 1
fi
