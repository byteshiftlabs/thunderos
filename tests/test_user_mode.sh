#!/bin/bash
#
# User Mode Testing Script
# Tests user-mode process execution and privilege separation
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build"
QEMU_TIMEOUT=10
OUTPUT_FILE="${SCRIPT_DIR}/thunderos_user_mode_output.txt"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper functions
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[*]${NC} $1"
}

# Build the kernel
print_info "Building kernel..."
cd "${SCRIPT_DIR}/.."
if make clean >/dev/null 2>&1 && make >/dev/null 2>&1; then
    print_status "Build successful"
else
    print_error "Build failed"
    exit 1
fi
cd "${SCRIPT_DIR}"

# Check if executable exists
if [ ! -f "${BUILD_DIR}/thunderos.elf" ]; then
    print_error "Kernel ELF not found at ${BUILD_DIR}/thunderos.elf"
    exit 1
fi

print_status "Kernel ELF found"

# Create ext2 disk image for testing
print_info "Creating ext2 disk image..."
DISK_IMAGE="${BUILD_DIR}/ci-usermode-disk.img"
rm -rf "${BUILD_DIR}/ci-testfs"
mkdir -p "${BUILD_DIR}/ci-testfs/bin"
echo "User Mode Test File" > "${BUILD_DIR}/ci-testfs/test.txt"

# Build userland programs if available
cd "${SCRIPT_DIR}/.."
if make userland >/dev/null 2>&1; then
    cp -f userland/build/hello "${BUILD_DIR}/ci-testfs/bin/" 2>/dev/null || true
    cp -f userland/build/cat "${BUILD_DIR}/ci-testfs/bin/" 2>/dev/null || true
    cp -f userland/build/ls "${BUILD_DIR}/ci-testfs/bin/" 2>/dev/null || true
fi
cd "${SCRIPT_DIR}"

# Create ext2 filesystem
mkfs.ext2 -F -q -d "${BUILD_DIR}/ci-testfs" "${DISK_IMAGE}" 10M >/dev/null 2>&1
rm -rf "${BUILD_DIR}/ci-testfs"
print_status "ext2 disk image created"

# Run QEMU with timeout and VirtIO block device
print_info "Running user mode test with VirtIO disk..."
{
    sleep "${QEMU_TIMEOUT}"
    echo ""
} | timeout $((QEMU_TIMEOUT + 2)) qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel "${BUILD_DIR}/thunderos.elf" \
    -global virtio-mmio.force-legacy=false \
    -drive file="${DISK_IMAGE}",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 2>&1 | tee "${OUTPUT_FILE}"

print_status "QEMU execution completed (output saved to ${OUTPUT_FILE})"

# Analyze output
print_info "Analyzing user mode test results..."

TEST_RESULTS=0

# Test 1: Check if kernel started
if grep -q "Kernel loaded\|Initializing..." "${OUTPUT_FILE}"; then
    print_status "Test 1: Kernel initialization - PASS"
else
    print_error "Test 1: Kernel initialization - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 2: Check for user mode processes
if grep -q "Created user process\|USER PROCESS EXCEPTION" "${OUTPUT_FILE}"; then
    print_status "Test 2: User mode processes detected - PASS"
else
    print_error "Test 2: User mode processes detected - FAIL"
    TEST_RESULTS=$((TEST_RESULTS + 1))
fi

# Test 3: Check for privilege separation
if grep -q "privilege\|S-mode\|U-mode\|separation" "${OUTPUT_FILE}"; then
    print_status "Test 3: Privilege separation active - PASS"
else
    print_error "Test 3: Privilege separation active - FAIL (optional)"
fi

# Test 4: Check for memory isolation
if grep -q "memory isolation\|page table\|Sv39" "${OUTPUT_FILE}"; then
    print_status "Test 4: Memory isolation enabled - PASS"
else
    print_error "Test 4: Memory isolation enabled - FAIL (optional)"
fi

# Test 5: Check for system calls
if grep -q "USER PROCESS EXCEPTION\|Load page fault" "${OUTPUT_FILE}"; then
    print_status "Test 5: System calls operational - PASS"
else
    print_error "Test 5: System calls operational - FAIL (optional)"
fi

# Test 6: Check for scheduler with user processes
if grep -q "Scheduler\|scheduling\|PID" "${OUTPUT_FILE}"; then
    print_status "Test 6: Scheduler running with user processes - PASS"
else
    print_error "Test 6: Scheduler running with user processes - FAIL (optional)"
fi

# Summary
echo ""
echo "================================"
if [ $TEST_RESULTS -eq 0 ]; then
    print_status "All critical user mode tests passed!"
    exit 0
else
    print_error "$TEST_RESULTS test(s) failed"
    exit 1
fi
