# ThunderOS

A lightweight RISC-V operating system for educational use, providing a clean foundation for OS development and embedded systems experimentation.

## Current Status

**Version 0.10.0 - "System Control"** 🎯 Released!

- ✅ **v0.10.0 Released** - Graceful shutdown and reboot from user space
- ✅ `poweroff` and `reboot` utilities available in userland
- ✅ SBI SRST support with legacy and QEMU test-device fallbacks
- ✅ Prior process, filesystem, shell, and synchronization features remain available
- 🚧 **Next**: VirtIO-net driver, TCP/IP stack (v0.11.0 Networking)

See [CHANGELOG.md](CHANGELOG.md) for complete feature list and [ROADMAP.md](ROADMAP.md) for future plans.

## Quick Start

### Cloning
```bash
git clone --recurse-submodules https://github.com/byteshiftlabs/thunderos.git
cd thunderos
git submodule update --init --recursive
```

### Authoritative Verification
```bash
make docker-verify
```

This is the release-grade setup path. It builds the repository Docker image, uses the pinned RISC-V toolchain and QEMU 10.1.2 from that image, and runs `make clean && make && make test` inside the container.

### Building
```bash
make clean && make
```

For an opt-in faster guest build while keeping symbols:
```bash
make OPT_LEVEL=-O2 DEBUG_SYMBOLS=1
```

### Running in QEMU
```bash
make qemu
```

The OS will automatically build the filesystem image and start QEMU with VirtIO block device support. Native QEMU runs are a convenience path for local iteration; if your host toolchain or QEMU version differs from the Docker environment, use `make docker-verify` before treating the result as release-quality.

For smoother local runs, you can override QEMU acceleration and extra flags:
```bash
QEMU_ACCEL_FLAGS='-accel tcg,thread=multi' QEMU_EXTRA_FLAGS='' ./run_os.sh
```

### Automated Testing
```bash
# Run all tests
make test

# Individual test scripts
cd tests/scripts
./test_boot.sh          # Quick boot validation
./test_kernel.sh        # Comprehensive kernel test
./test_integration.sh   # Full integration tests
./run_all_tests.sh      # Run all test suites
```

### Debugging
```bash
make debug
# In another terminal:
riscv64-unknown-elf-gdb build/thunderos.elf
(gdb) target remote :1234
```

If your host toolchain does not match the Docker-verified environment, open the authoritative shell first and debug from there:

```bash
make docker-shell
```

## Setup Contract

ThunderOS has two supported public setup paths:

1. `make docker-verify`
  This is the authoritative verification path used to mirror release gating. It relies on the repository Dockerfile, which pins the RISC-V bare-metal toolchain and QEMU 10.1.2.
2. `make`, `make qemu`, `make debug`
  This is the native Linux convenience path for day-to-day development when you already have a matching toolchain, `mkfs.ext2`, and QEMU 10.1.2+ on the host.

If the native host path disagrees with Docker, Docker wins.

## Versioning And Userland

`external/userland/` is pinned by git submodule commit, not by a floating branch. Releases are expected to update that submodule intentionally, keep `git submodule status` clean, and pass `make docker-verify` before publishing. That is the contract that keeps kernel and userland changes versioned together.

## Documentation

Full technical documentation is available in Sphinx format:

```bash
cd docs
make html
# Open docs/build/html/index.html in browser
```

## Project Structure
```
boot/                - Bootloader and early initialization
kernel/              - Kernel core
  arch/riscv64/      - RISC-V architecture-specific code
    drivers/         - RISC-V HAL implementations (UART, timer, etc.)
    interrupt/       - Trap/interrupt handling
  core/              - Portable kernel core (process, scheduler, shell, ELF loader)
  drivers/           - Device drivers (VirtIO block, etc.)
  fs/                - Filesystem implementations (VFS, ext2)
  mm/                - Memory management (PMM, kmalloc, paging, DMA)
include/             - Header files
  hal/               - Hardware Abstraction Layer interfaces
  kernel/            - Kernel subsystem headers
  fs/                - Filesystem headers (VFS, ext2)
  arch/              - Architecture-specific headers (barriers, etc.)
  mm/                - Memory management headers (DMA, paging)
docs/                - Sphinx documentation
tests/               - Test framework and test cases
external/            - External repositories and submodules
  userland/          - User-space programs
build/               - Build output
```

## Development

See [ROADMAP.md](ROADMAP.md) for the development roadmap from v0.1 through v2.0.

See [docs/source/development/code_quality.rst](docs/source/development/code_quality.rst) for coding standards.

## Testing

### Test Framework
The project includes an automated test suite:

```bash
# Run all tests
make test

# Or manually run individual tests
cd tests/scripts
./test_boot.sh          # Boot sequence validation
./test_integration.sh   # Full integration tests
./test_kernel.sh        # Comprehensive kernel test suite
```

Test suite validates:
- ✓ Memory management (PMM, kmalloc, paging, DMA)
- ✓ Memory isolation (per-process page tables, VMAs, heap safety)
- ✓ Address translation (virt↔phys)
- ✓ Memory barriers (fence instructions)
- ✓ Kernel initialization and boot sequence
- ✓ Process creation and scheduling
- ✓ User-space syscalls (brk, mmap, munmap, fork)
- ✓ Memory protection and isolation
- ✓ VirtIO block device I/O
- ✓ ext2 filesystem operations
- ✓ ELF program loading and execution

### User-Space Programs

Located in `external/userland/`:
- **Core utilities**: cat, ls, pwd, mkdir, rmdir, touch, rm, clear, sleep
- **System utilities**: ps, uname, uptime, whoami, tty, kill
- **Shell**: ush (interactive shell with command history)
- **Test programs**: hello, clock, signal_test, pipe_test, fork_test

Programs are compiled as RISC-V ELF64 executables and can be loaded from the ext2 filesystem.

## Platform Support

- **Runtime target**: QEMU `virt` machine with 128 MiB RAM, `-bios none`, and VirtIO block storage
- **Authoritative verification environment**: Repository Dockerfile with the pinned RISC-V toolchain and QEMU 10.1.2
- **Native host development**: Supported as a convenience path when the host matches the documented toolchain requirements
- **Real hardware**: Not a supported public target yet; bring-up work should be treated as experimental until documented otherwise

## Requirements

- RISC-V GNU Toolchain (`riscv64-unknown-elf-gcc`)
- QEMU 10.1.2+ RISC-V System Emulator (`qemu-system-riscv64`)
  - ThunderOS uses `-bios none` on the supported QEMU path, so no OpenSBI runtime is required
  - ACLINT timer device
- Make
- Standard Unix utilities (bash, sed, etc.)
- For building QEMU: ninja, glib-2.0, pixman, slirp

## License

See [LICENSE](LICENSE) file for details.
