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

source "${SCRIPT_DIR}/structured_test_helpers.sh"

# ── Header ───────────────────────────────────────────────────────────────────

_VERSION=$(cd "${ROOT_DIR}" && git describe --tags --always --dirty 2>/dev/null || echo "unknown")
_BRANCH=$(cd "${ROOT_DIR}" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
_COMMIT=$(cd "${ROOT_DIR}" && git rev-parse --short HEAD 2>/dev/null || echo "unknown")
_DATE=$(date '+%Y-%m-%d %H:%M:%S')

printf "\n"
printf "${_B}╔══════════════════════════════════════════════════════════════╗${_N}\n"
printf "${_B}║${_N}           ThunderOS — RISC-V Kernel Test Suite               ${_B}║${_N}\n"
printf "${_B}╠══════════════════════════════════════════════════════════════╣${_N}\n"
printf "${_B}║${_N}  Branch  : %-50s${_B}║${_N}\n" "${_BRANCH}"
printf "${_B}║${_N}  Commit  : %-50s${_B}║${_N}\n" "${_COMMIT} (${_VERSION})"
printf "${_B}║${_N}  Date    : %-50s${_B}║${_N}\n" "${_DATE}"
printf "${_B}║${_N}%-62s${_B}║${_N}\n" "  Target  : QEMU virt / RISC-V rv64gc / Sv39 paging"
printf "${_B}╠══════════════════════════════════════════════════════════════╣${_N}\n"
printf "${_B}║${_N}%-62s${_B}║${_N}\n" "  [unit]  Boot / Kernel (PMM, kmalloc, errno, ELF, memory)"
printf "${_B}║${_N}%-62s${_B}║${_N}\n" "  [intg]  Boot / Kernel (VirtIO, ext2, VFS) / Integration"
printf "${_B}╚══════════════════════════════════════════════════════════════╝${_N}\n"
printf "\n"

SUITES_FAILED=0
SUITES_TOTAL=0
GRAND_PASS=0
GRAND_FAIL=0

_accum_counts() {
    local f="$1"
    [ -f "$f" ] || return
    while read -r key val; do
        case "$key" in
            PASS) GRAND_PASS=$((GRAND_PASS + val)) ;;
            FAIL) GRAND_FAIL=$((GRAND_FAIL + val)) ;;
        esac
    done < "$f"
}

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
    printf " FAILED\n"
    printf "[${_R}  ERROR   ${_N}] Userland build failed. Ensure the submodule is initialized.\n" >&2
    exit 1
fi

OUTPUT_DIR="${ROOT_DIR}/tests/outputs"
mkdir -p "${OUTPUT_DIR}"

run_suite "Kernel Functionality" "test_kernel.sh" --skip-build || true
_accum_counts "${OUTPUT_DIR}/.kernel_counts"

run_suite "Integration" "test_integration.sh" --skip-build || true
_accum_counts "${OUTPUT_DIR}/.integration_counts"

GRAND_TOTAL=$((GRAND_PASS + GRAND_FAIL))
printf "\n[${_B}==========${_N}] %d suite(s) ran. %d/%d tests passed.\n" \
    "$SUITES_TOTAL" "$GRAND_PASS" "$GRAND_TOTAL"
if [ $SUITES_FAILED -eq 0 ]; then
    printf "[${_G}  PASSED  ${_N}] All %d test(s) across %d suite(s).\n" "$GRAND_PASS" "$SUITES_TOTAL"
    exit 0
else
    printf "[${_R}  FAILED  ${_N}] %d test(s) failed across %d of %d suite(s).\n" \
        "$GRAND_FAIL" "$SUITES_FAILED" "$SUITES_TOTAL"
    exit 1
fi
