#!/bin/bash
# Test script for VirtIO block driver

set -e

echo "==================================================="
echo "  ThunderOS VirtIO Block Driver Test"
echo "==================================================="

# Check if toolchain is available
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "ERROR: riscv64-unknown-elf-gcc not found"
    echo "Please install RISC-V toolchain first"
    exit 1
fi

# Build kernel
echo ""
echo "Building kernel..."
make clean
make

# Create test disk if it doesn't exist
if [ ! -f build/test-disk.img ]; then
    echo ""
    echo "Creating test disk image (10MB)..."
    dd if=/dev/zero of=build/test-disk.img bs=1M count=10
fi

# Run QEMU with VirtIO block device
echo ""
echo "Running QEMU with VirtIO block device..."
echo "The kernel will run VirtIO block driver tests automatically."
echo "Press Ctrl+A, X to exit QEMU"
echo ""

timeout 10s qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -drive file=build/test-disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    || true

echo ""
echo "==================================================="
echo "  Test completed"
echo "==================================================="
