#!/bin/bash
# ThunderOS Integration Test
# Verifies system stack end-to-end with userland binaries on the filesystem.
#
# Options:
#   --skip-build   Skip make; use existing build/thunderos.elf

export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/integration_test_output.txt"
QEMU_TIMEOUT=8
SKIP_BUILD=0

for arg in "$@"; do
    [ "$arg" = "--skip-build" ] && SKIP_BUILD=1
done

# shellcheck source=test_helpers.sh
source "${SCRIPT_DIR}/test_helpers.sh"

if command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="qemu-system-riscv64"
elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
    QEMU_BIN="/tmp/qemu-10.1.2/build/qemu-system-riscv64"
else
    printf "[${_R}  ERROR   ${_N}] qemu-system-riscv64 not found\n" >&2
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

printf "[${_B}==========${_N}] ThunderOS Integration Test\n"

# ── Setup ───────────────────────────────────────────────────────────────────

if [ "$SKIP_BUILD" -eq 0 ]; then
    printf "[ SETUP    ] Building kernel (ENABLE_TESTS=1 TEST_MODE=1)..."
    cd "${ROOT_DIR}"
    if make clean >/dev/null 2>&1 && make ENABLE_TESTS=1 TEST_MODE=1 >/dev/null 2>&1; then
        printf " OK\n"
    else
        printf " FAILED\n"
        printf "[${_R}  ERROR   ${_N}] Kernel build failed.\n" >&2
        exit 1
    fi

    printf "[ SETUP    ] Building userland..."
    if make userland >/dev/null 2>&1; then
        printf " OK\n"
    else
        printf " skipped\n"
    fi
fi

printf "[ SETUP    ] Creating ext2 image with userland binaries..."
DISK_IMAGE="${BUILD_DIR}/integration_fs.img"
FS_DIR="${BUILD_DIR}/_integration_fs"
rm -rf "${FS_DIR}"
mkdir -p "${FS_DIR}/bin"
printf "Hello from ThunderOS!\n" > "${FS_DIR}/hello.txt"
COPIED=0
for prog in hello cat ls; do
    if [ -f "${ROOT_DIR}/userland/build/$prog" ]; then
        cp "${ROOT_DIR}/userland/build/$prog" "${FS_DIR}/bin/"
        COPIED=$((COPIED + 1))
    fi
done
if mkfs.ext2 -F -q -d "${FS_DIR}" "${DISK_IMAGE}" 10M >/dev/null 2>&1; then
    printf " OK (%d binaries)\n" "$COPIED"
    rm -rf "${FS_DIR}"
else
    printf " FAILED\n"
    printf "[${_R}  ERROR   ${_N}] mkfs.ext2 failed\n" >&2
    exit 1
fi

printf "[ SETUP    ] Running QEMU (%ds timeout)..." "${QEMU_TIMEOUT}"
T0=$SECONDS
timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
    -machine virt -m 128M -nographic -serial mon:stdio -bios none \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    -global virtio-mmio.force-legacy=false \
    -drive file="${DISK_IMAGE}",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    </dev/null >"${OUTPUT_FILE}" 2>&1 || true
T_ELAPSED=$((SECONDS - T0))
printf " done (%ds)\n\n" "${T_ELAPSED}"

# ── Tests ───────────────────────────────────────────────────────────────────

gtest_suite_begin "Integration"
gtest_run "Integration.KernelStarted"  "\[OK\] UART initialized"          "${OUTPUT_FILE}"
gtest_run "Integration.MemoryTests"    "ALL TESTS PASSED"                 "${OUTPUT_FILE}"
gtest_run "Integration.VirtIOReady"    "\[OK\] VirtIO block device"       "${OUTPUT_FILE}"
gtest_run "Integration.Ext2Mounted"    "\[OK\] ext2 filesystem mounted"   "${OUTPUT_FILE}"
gtest_run "Integration.ElfLoaderTests" "ELF Loader Tests"                 "${OUTPUT_FILE}"
gtest_suite_end "Integration"

gtest_summary "${T_ELAPSED}"
RESULT=$?
gtest_export_counts "${OUTPUT_DIR}/.integration_counts"
printf "  Full output: %s\n" "${OUTPUT_FILE}"
exit $RESULT
