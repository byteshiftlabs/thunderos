#!/usr/bin/env bash
#
# Run ThunderOS in QEMU
#
# Builds the kernel if needed, then launches QEMU with proper configuration.
# Press Ctrl+A then X to exit QEMU.

set -euo pipefail

readonly KERNEL_ELF="build/thunderos.elf"
readonly FS_IMG="build/fs.img"

main() {
    # Build if kernel doesn't exist
    if [[ ! -f "${KERNEL_ELF}" ]]; then
        echo "Kernel not found, building..."
        make all
    fi
    
    # Create filesystem if it doesn't exist
    if [[ ! -f "${FS_IMG}" ]]; then
        echo "Filesystem image not found, running make fs..."
        make fs
    fi
    
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Starting ThunderOS in QEMU"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Kernel: ${KERNEL_ELF}"
    echo "  Filesystem: ${FS_IMG}"
    echo ""
    echo "  Press Ctrl+A then X to exit QEMU"
    echo "  Press ESC+1 or ESC+2 to switch terminals"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    # Check if qemu-system-riscv64 exists
    if ! command -v qemu-system-riscv64 >/dev/null 2>&1; then
        echo "✗ ERROR: qemu-system-riscv64 not found" >&2
        echo "Please install QEMU for RISC-V or run in Docker" >&2
        exit 1
    fi
    
    # Run QEMU with correct flags
    exec qemu-system-riscv64 \
        -machine virt \
        -m 128M \
        -nographic \
        -serial mon:stdio \
        -bios none \
        -kernel "${KERNEL_ELF}" \
        -global virtio-mmio.force-legacy=false \
        -drive file="${FS_IMG}",if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0
}

main "$@"
