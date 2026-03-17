#!/bin/bash
# ThunderOS Kernel Functionality Test
# Builds kernel (ENABLE_TESTS=1 TEST_MODE=1), runs in QEMU, checks output.
#
# Options:
#   --skip-build   Skip make; use existing build/thunderos.elf

export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/kernel_test_output.txt"
QEMU_TIMEOUT=10
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
        printf " FAILED\n"
        printf "[${_R}  ERROR   ${_N}] Userland build failed. Ensure the submodule is initialized.\n" >&2
        exit 1
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

testfmt_suite_begin "Boot"
testfmt_run "Boot.BannerDisplayed"       "ThunderOS.*RISC-V"                "${OUTPUT_FILE}"
testfmt_run "Boot.UartInitialized"       "\[OK\] UART initialized"          "${OUTPUT_FILE}"
testfmt_run "Boot.InterruptsInitialized" "\[OK\] Interrupt subsystem"       "${OUTPUT_FILE}"
testfmt_run "Boot.TrapHandlerInstalled"  "\[OK\] Trap handler"              "${OUTPUT_FILE}"
testfmt_run "Boot.MemoryManagement"      "\[OK\] Memory management"         "${OUTPUT_FILE}"
testfmt_run "Boot.VirtualMemory"         "\[OK\] Virtual memory"            "${OUTPUT_FILE}"
testfmt_run "Boot.DmaAllocator"          "\[OK\] DMA allocator"             "${OUTPUT_FILE}"
testfmt_suite_end "Boot"

testfmt_suite_begin "Memory"
testfmt_run "Memory.FeatureSuitePassed"   "\[----------\] 10 test[(]s[)] from MemoryFeatures"          "${OUTPUT_FILE}"
testfmt_run "Memory.IsolationSuitePassed" "\[----------\] 15 test[(]s[)] from MemoryIsolation"        "${OUTPUT_FILE}"
testfmt_suite_end "Memory"

testfmt_suite_begin "ELF"
testfmt_run "ELF.LoaderTestsPassed"      "ELFLoader\.FreshElfProcessGetsArgvStack" "${OUTPUT_FILE}"
testfmt_suite_end "ELF"

testfmt_suite_begin "Process"
testfmt_run "Process.Initialized"        "\[OK\] Process management"        "${OUTPUT_FILE}"
testfmt_run "Process.SchedulerReady"     "\[OK\] Scheduler"                 "${OUTPUT_FILE}"
testfmt_run "Process.PipeSubsystem"      "\[OK\] Pipe subsystem"            "${OUTPUT_FILE}"
testfmt_suite_end "Process"

testfmt_suite_begin "VirtIO"
testfmt_run "VirtIO.BlockDeviceReady"    "\[OK\] VirtIO block device"       "${OUTPUT_FILE}"
testfmt_suite_end "VirtIO"

testfmt_suite_begin "Filesystem"
testfmt_run "FS.Ext2Mounted"             "\[OK\] ext2 filesystem mounted"   "${OUTPUT_FILE}"
testfmt_run "FS.VfsRootMounted"          "\[OK\] VFS root filesystem"       "${OUTPUT_FILE}"
testfmt_suite_end "Filesystem"

testfmt_suite_begin "UnitTests"
testfmt_run "UnitTests.PMM"              "PMM\.FreeCountRestoredToInitialAfterAllFrees" "${OUTPUT_FILE}"
testfmt_run "UnitTests.Kmalloc"          "Kmalloc\.PageAlignedAllocationIsPageAligned" "${OUTPUT_FILE}"
testfmt_run "UnitTests.Errno"            "Errno\.ErrnoSetClearCycleIsStable" "${OUTPUT_FILE}"
testfmt_run "UnitTests.WaitQueue"        "WaitQueue\.SleepReturnsEnomemOnForcedAllocationFailure" "${OUTPUT_FILE}"
testfmt_run "UnitTests.SyncPrimitives"   "SyncPrimitives\.ReadTrylockRespectsWaitingWriterPriority" "${OUTPUT_FILE}"
testfmt_run "UnitTests.Syscalls"         "Syscalls\.UnknownSyscallReturnsMinusOne" "${OUTPUT_FILE}"
testfmt_run "UnitTests.Pipes"            "Pipes\.WriteWithClosedReaderSetsEpipe" "${OUTPUT_FILE}"
testfmt_run "UnitTests.SchedulerSoak"    "SchedulerSoak\.WorkersCompleteWithinBudget" "${OUTPUT_FILE}"
testfmt_suite_end "UnitTests"

testfmt_suite_begin "Stability"
testfmt_run_absent "Stability.NoPanic"   "KERNEL PANIC\|unhandled trap\|double fault" "${OUTPUT_FILE}"
testfmt_suite_end "Stability"

testfmt_summary "${T_ELAPSED}"
RESULT=$?
testfmt_export_counts "${OUTPUT_DIR}/.kernel_counts"
printf "  Full output: %s\n" "${OUTPUT_FILE}"
exit $RESULT
