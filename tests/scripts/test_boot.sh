#!/bin/bash
#
# ThunderOS Boot Test
# Quick test to verify kernel boots and initializes correctly
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#

set -e

# Ensure TERM is set for tput commands (needed for CI environments)
export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
OUTPUT_FILE="${OUTPUT_DIR}/boot_test_output.txt"
DISK_IMAGE="${BUILD_DIR}/boot_test_fs.img"
QEMU_TIMEOUT=5

# QEMU 10.1.2+ required for SSTC extension support
# Try system QEMU first, then custom build
if command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="${QEMU_BIN:-qemu-system-riscv64}"
elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
    QEMU_BIN="/tmp/qemu-10.1.2/build/qemu-system-riscv64"
else
    echo "ERROR: qemu-system-riscv64 not found"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo "========================================"
    echo "  $1"
    echo "========================================"
    echo ""
}

print_pass() {
    echo -e "  ${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "  ${RED}[FAIL]${NC} $1"
}

print_info() {
    echo -e "  ${BLUE}[INFO]${NC} $1"
}

print_test() {
    echo -e "\n${YELLOW}[TEST]${NC} $1"
}

require_command() {
    local command_name="$1"
    local install_hint="$2"

    if command -v "$command_name" >/dev/null 2>&1; then
        return 0
    fi

    print_fail "Required command not found: ${command_name}"
    print_info "${install_hint}"
    exit 1
}

create_boot_filesystem() {
    local fs_root="${BUILD_DIR}/boot_test_fs_contents"

    print_info "Building userland programs..."
    if make userland >/dev/null 2>&1; then
        print_pass "Userland build successful"
    else
        print_fail "Userland build failed"
        exit 1
    fi

    print_info "Creating ext2 filesystem image..."
    rm -rf "${fs_root}" "${DISK_IMAGE}"
    mkdir -p "${fs_root}/bin"

    echo "ThunderOS boot test filesystem" > "${fs_root}/README.txt"
    echo "hello from boot test" > "${fs_root}/hello.txt"

    local required_programs="ush ls cat pwd clear"
    for prog in ${required_programs}; do
        if [ ! -f "${ROOT_DIR}/external/userland/build/${prog}" ]; then
            print_fail "Missing required userland binary: /bin/${prog}"
            exit 1
        fi
        cp "${ROOT_DIR}/external/userland/build/${prog}" "${fs_root}/bin/"
    done

    if mkfs.ext2 -F -q -d "${fs_root}" "${DISK_IMAGE}" 10M >/dev/null 2>&1; then
        print_pass "Boot filesystem image created"
    else
        print_fail "ext2 filesystem creation failed"
        exit 1
    fi

    rm -rf "${fs_root}"
}

# Build kernel
print_header "ThunderOS Boot Test"
print_info "Building kernel..."

require_command mkfs.ext2 "Install e2fsprogs: sudo apt-get install e2fsprogs"

cd "${ROOT_DIR}"
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_pass "Kernel build successful"
else
    print_fail "Kernel build failed"
    exit 1
fi

# Verify ELF exists
if [ ! -f "${BUILD_DIR}/thunderos.elf" ]; then
    print_fail "Kernel ELF not found"
    exit 1
fi
print_pass "Kernel ELF verified"

create_boot_filesystem

# Run QEMU
print_test "Booting kernel in QEMU (${QEMU_TIMEOUT}s timeout)"

timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios none \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    -global virtio-mmio.force-legacy=false \
    -drive file="${DISK_IMAGE}",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    </dev/null 2>&1 | tee "${OUTPUT_FILE}" || true

# Analyze output
print_test "Analyzing boot sequence"

FAILED=0

# Test 1: Kernel boots
if grep -q "ThunderOS\|Kernel loaded" "${OUTPUT_FILE}"; then
    print_pass "Kernel started"
else
    print_fail "Kernel did not start"
    FAILED=$((FAILED + 1))
fi

# Test 2: UART initialized
if grep -q "\[OK\] UART initialized" "${OUTPUT_FILE}"; then
    print_pass "UART initialized"
else
    print_fail "UART initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 3: Interrupts enabled
if grep -q "\[OK\] Interrupt\|interrupts enabled" "${OUTPUT_FILE}"; then
    print_pass "Interrupts enabled"
else
    print_fail "Interrupt subsystem failed"
    FAILED=$((FAILED + 1))
fi

# Test 4: Memory management
if grep -q "\[OK\] Memory management\|PMM: Initialized" "${OUTPUT_FILE}"; then
    print_pass "Memory management initialized"
else
    print_fail "Memory management failed"
    FAILED=$((FAILED + 1))
fi

# Test 5: Virtual memory
if grep -q "\[OK\] Virtual memory\|Paging enabled" "${OUTPUT_FILE}"; then
    print_pass "Virtual memory enabled"
else
    print_fail "Virtual memory failed"
    FAILED=$((FAILED + 1))
fi

# Test 6: Process management
if grep -q "\[OK\] Process management\|Scheduler initialized" "${OUTPUT_FILE}"; then
    print_pass "Process and scheduler initialized"
else
    print_fail "Process/scheduler initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 7: VirtIO block device and ext2 mount
if grep -q "\[OK\] ext2 filesystem mounted successfully\|\[OK\] VFS root filesystem mounted" "${OUTPUT_FILE}"; then
    print_pass "VirtIO block device and filesystem initialized"
else
    print_fail "VirtIO block device or filesystem initialization failed"
    FAILED=$((FAILED + 1))
fi

# Test 8: User shell launch
if grep -q "\[OK\] Shell on VT1" "${OUTPUT_FILE}"; then
    print_pass "User shell launched from filesystem"
else
    print_fail "User shell did not launch"
    FAILED=$((FAILED + 1))
fi

# Summary
print_header "Boot Test Summary"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All boot tests passed (8/8)${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 0
else
    echo -e "${RED}✗ $FAILED test(s) failed${NC}"
    print_info "Output saved to: ${OUTPUT_FILE}"
    exit 1
fi
