#!/bin/bash
# ThunderOS Boot Test
# Quick sanity check: builds kernel (no tests), boots in QEMU, verifies init.
#
# Options:
#   --skip-build   Skip make; use existing build/thunderos.elf

export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/boot_test_output.txt"
QEMU_TIMEOUT=5
SKIP_BUILD=0

for arg in "$@"; do
    [ "$arg" = "--skip-build" ] && SKIP_BUILD=1
done

# shellcheck source=structured_test_helpers.sh
source "${SCRIPT_DIR}/structured_test_helpers.sh"

if ! testfmt_select_qemu 10; then
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

printf "[${_B}==========${_N}] ThunderOS Boot Test\n"

# ── Setup ───────────────────────────────────────────────────────────────────

if [ "$SKIP_BUILD" -eq 0 ]; then
    printf "[ SETUP    ] Building kernel..."
    cd "${ROOT_DIR}"
    if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
        printf " OK\n"
    else
        printf " FAILED\n"
        printf "[${_R}  ERROR   ${_N}] Kernel build failed.\n" >&2
        exit 1
    fi
fi

printf "[ SETUP    ] Running QEMU (%ds timeout)..." "${QEMU_TIMEOUT}"
T0=$SECONDS
timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
    -machine virt -m 128M -nographic -serial mon:stdio -bios none \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    </dev/null >"${OUTPUT_FILE}" 2>&1 || true
T_ELAPSED=$((SECONDS - T0))
printf " done (%ds)\n\n" "${T_ELAPSED}"

# ── Tests ───────────────────────────────────────────────────────────────────

testfmt_suite_begin "Boot"
testfmt_run "Boot.BannerDisplayed"   "ThunderOS\|Kernel loaded"            "${OUTPUT_FILE}"
testfmt_run "Boot.UartInitialized"   "\[OK\] UART initialized"             "${OUTPUT_FILE}"
testfmt_run "Boot.Interrupts"        "\[OK\] Interrupt\|interrupts enabled" "${OUTPUT_FILE}"
testfmt_run "Boot.MemoryManagement"  "\[OK\] Memory management\|PMM: Initialized" "${OUTPUT_FILE}"
testfmt_run "Boot.VirtualMemory"     "\[OK\] Virtual memory\|Paging enabled" "${OUTPUT_FILE}"
testfmt_run "Boot.ProcessScheduler"  "\[OK\] Process management\|Scheduler" "${OUTPUT_FILE}"
testfmt_suite_end "Boot"

testfmt_summary "${T_ELAPSED}"
RESULT=$?
printf "  Full output: %s\n" "${OUTPUT_FILE}"
exit $RESULT
