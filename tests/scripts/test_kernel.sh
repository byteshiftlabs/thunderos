#!/bin/bash
# ThunderOS Kernel Functionality Test
# Builds kernel (ENABLE_TESTS=1 TEST_MODE=1), runs in QEMU, checks output.
#
# Options:
#   --skip-build   Skip make; use existing build/thunderos.elf

export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/kernel_test_output.txt"
QEMU_TIMEOUT=10
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

printf "[${_B}==========${_N}] ThunderOS Kernel Functionality Test\n"

# ── Setup ───────────────────────────────────────────────────────────────────

if [ "$SKIP_BUILD" -eq 0 ]; then
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
fi

printf "[ SETUP    ] Creating ext2 test image..."
DISK_IMAGE="${BUILD_DIR}/kernel_test_fs.img"
rm -rf "${BUILD_DIR}/_kernel_test_fs"
mkdir -p "${BUILD_DIR}/_kernel_test_fs/bin"
printf "Test file\n"         > "${BUILD_DIR}/_kernel_test_fs/test.txt"
printf "Hello from ext2!\n"  > "${BUILD_DIR}/_kernel_test_fs/hello.txt"
for prog in hello cat ls pwd mkdir rmdir clear ush; do
    [ -f "${ROOT_DIR}/userland/build/$prog" ] && \
        cp "${ROOT_DIR}/userland/build/$prog" "${BUILD_DIR}/_kernel_test_fs/bin/"
done
if mkfs.ext2 -F -q -d "${BUILD_DIR}/_kernel_test_fs" "${DISK_IMAGE}" 10M >/dev/null 2>&1; then
    printf " OK\n"
    rm -rf "${BUILD_DIR}/_kernel_test_fs"
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

gtest_suite_begin "Boot"
gtest_run "Boot.BannerDisplayed"       "ThunderOS.*RISC-V"                "${OUTPUT_FILE}"
gtest_run "Boot.UartInitialized"       "\[OK\] UART initialized"          "${OUTPUT_FILE}"
gtest_run "Boot.InterruptsInitialized" "\[OK\] Interrupt subsystem"       "${OUTPUT_FILE}"
gtest_run "Boot.TrapHandlerInstalled"  "\[OK\] Trap handler"              "${OUTPUT_FILE}"
gtest_run "Boot.MemoryManagement"      "\[OK\] Memory management"         "${OUTPUT_FILE}"
gtest_run "Boot.VirtualMemory"         "\[OK\] Virtual memory"            "${OUTPUT_FILE}"
gtest_run "Boot.DmaAllocator"          "\[OK\] DMA allocator"             "${OUTPUT_FILE}"
gtest_suite_end "Boot"

gtest_suite_begin "Memory"
gtest_run "Memory.BuiltinTestsPassed"  "ALL TESTS PASSED"                 "${OUTPUT_FILE}"
gtest_suite_end "Memory"

gtest_suite_begin "ELF"
gtest_run "ELF.LoaderTestsPassed"      "ELF Loader Tests"                 "${OUTPUT_FILE}"
gtest_suite_end "ELF"

gtest_suite_begin "Process"
gtest_run "Process.Initialized"        "\[OK\] Process management"        "${OUTPUT_FILE}"
gtest_run "Process.SchedulerReady"     "\[OK\] Scheduler"                 "${OUTPUT_FILE}"
gtest_run "Process.PipeSubsystem"      "\[OK\] Pipe subsystem"            "${OUTPUT_FILE}"
gtest_suite_end "Process"

gtest_suite_begin "VirtIO"
gtest_run "VirtIO.BlockDeviceReady"    "\[OK\] VirtIO block device"       "${OUTPUT_FILE}"
gtest_suite_end "VirtIO"

gtest_suite_begin "Filesystem"
gtest_run "FS.Ext2Mounted"             "\[OK\] ext2 filesystem mounted"   "${OUTPUT_FILE}"
gtest_run "FS.VfsRootMounted"          "\[OK\] VFS root filesystem"       "${OUTPUT_FILE}"
gtest_suite_end "Filesystem"

gtest_suite_begin "UnitTests"
gtest_run "UnitTests.PMM"              "PMM Unit Tests"                   "${OUTPUT_FILE}"
gtest_run "UnitTests.Kmalloc"          "kmalloc Unit Tests"               "${OUTPUT_FILE}"
gtest_run "UnitTests.Errno"            "errno Unit Tests"                 "${OUTPUT_FILE}"
gtest_suite_end "UnitTests"

gtest_suite_begin "Stability"
gtest_run_absent "Stability.NoPanic"   "KERNEL PANIC\|unhandled trap\|double fault" "${OUTPUT_FILE}"
gtest_suite_end "Stability"

gtest_summary "${T_ELAPSED}"
RESULT=$?
gtest_export_counts "${OUTPUT_DIR}/.kernel_counts"
printf "  Full output: %s\n" "${OUTPUT_FILE}"
exit $RESULT
