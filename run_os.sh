#!/usr/bin/env bash
#
# Run ThunderOS in QEMU
#
# Builds the kernel if needed, then launches QEMU with proper configuration.
# Press Ctrl+A then X to exit QEMU.

set -euo pipefail

readonly KERNEL_ELF="build/thunderos.elf"
readonly FS_IMG="build/fs.img"
readonly MIN_QEMU_VERSION="10.1.2"

version_supported() {
    local version="$1"

    if [[ -z "${version}" ]]; then
        return 1
    fi

    [[ "$(printf '%s\n' "${MIN_QEMU_VERSION}" "${version}" | sort -V | head -1)" == "${MIN_QEMU_VERSION}" ]]
}

require_supported_qemu() {
    local qemu_bin
    local qemu_version

    if ! qemu_bin="$(command -v qemu-system-riscv64 2>/dev/null)"; then
        echo "✗ ERROR: qemu-system-riscv64 not found" >&2
        echo "ThunderOS supports QEMU ${MIN_QEMU_VERSION}+ only." >&2
        echo "Use ./run_os_docker.sh or make qemu-docker." >&2
        exit 1
    fi

    qemu_version="$(${qemu_bin} --version | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1 || true)"
    if ! version_supported "${qemu_version}"; then
        echo "✗ ERROR: Unsupported QEMU version: ${qemu_version:-unknown}" >&2
        echo "ThunderOS supports QEMU ${MIN_QEMU_VERSION}+ only." >&2
        echo "Host QEMU 6.x hangs during early boot on this kernel." >&2
        echo "Use ./run_os_docker.sh or make qemu-docker." >&2
        exit 1
    fi
}

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

    require_supported_qemu
    
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
