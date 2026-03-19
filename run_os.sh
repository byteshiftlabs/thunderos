#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}"
BUILD_DIR="${ROOT_DIR}/build"
KERNEL_ELF="${BUILD_DIR}/thunderos.elf"
FS_IMG="${BUILD_DIR}/fs.img"

find_qemu() {
    if [[ -n "${QEMU_BIN:-}" ]]; then
        if [[ -x "${QEMU_BIN}" ]]; then
            printf '%s\n' "${QEMU_BIN}"
            return 0
        fi

        echo "ERROR: QEMU_BIN is set but not executable: ${QEMU_BIN}" >&2
        exit 1
    fi

    if command -v qemu-system-riscv64 >/dev/null 2>&1; then
        printf '%s\n' "qemu-system-riscv64"
        return 0
    fi

    if [[ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]]; then
        printf '%s\n' "/tmp/qemu-10.1.2/build/qemu-system-riscv64"
        return 0
    fi

    echo "ERROR: qemu-system-riscv64 not found. Install QEMU 10.1.2+ or set QEMU_BIN." >&2
    exit 1
}

require_artifact() {
    local path="$1"
    local label="$2"

    if [[ -f "${path}" ]]; then
        return 0
    fi

    echo "ERROR: Missing ${label}: ${path}" >&2
    echo "Build ThunderOS first with ./build_os.sh or make qemu." >&2
    exit 1
}

main() {
    local qemu_bin

    qemu_bin="$(find_qemu)"
    require_artifact "${KERNEL_ELF}" "kernel ELF"
    require_artifact "${FS_IMG}" "filesystem image"

    echo "Starting ThunderOS with ${qemu_bin}"
    echo "  Kernel: ${KERNEL_ELF}"
    echo "  Disk:   ${FS_IMG}"

    exec "${qemu_bin}" \
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