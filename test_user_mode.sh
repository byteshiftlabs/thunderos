#!/bin/bash
# Test user mode execution

cd /workspace

# Build userland programs first
echo "Building userland programs..."
./build_userland.sh

# Start QEMU and feed commands
{
    sleep 3  # Wait for boot
    echo "ls /"
    sleep 2
    echo "cat /hello.txt"
    sleep 2
    echo "exit"
} | timeout 20 qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -nographic \
    -serial mon:stdio \
    -bios default \
    -kernel build/thunderos.elf \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/ext2-disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 2>&1 | tee test_output.txt

echo ""
echo "=== Test Results ==="
if grep -q "lost+found" test_output.txt; then
    echo "✓ ls command worked"
else
    echo "✗ ls command failed"
fi

if grep -q "Hello from ext2" test_output.txt; then
    echo "✓ cat command worked"
else
    echo "✗ cat command failed"
fi
