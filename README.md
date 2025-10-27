# ThunderOS

A RISC-V operating system specialized for AI workloads.

## Features (Planned)
- RISC-V 64-bit architecture
- Support for RISC-V Vector Extension (RVV)
- Optimized task scheduling for AI workloads
- Memory management for large models
- Hardware accelerator support

## Project Structure
```
boot/           - Bootloader and early initialization
kernel/         - Kernel core
  arch/riscv64/ - RISC-V architecture-specific code
  core/         - Core kernel functionality
  drivers/      - Device drivers
  mm/           - Memory management
include/        - Header files
lib/            - Utility libraries
build/          - Build output
```

## Building
```bash
make all
```

## Running in QEMU
```bash
make qemu
```

## Documentation

Full documentation is available in Sphinx format:

```bash
cd docs
make html
# Open docs/build/html/index.html in browser
```

Online: See `docs/build/html/index.html`

## Development Roadmap
1. ✓ Project setup
2. ✓ Bootloader and kernel entry
3. ✓ UART driver
4. ✓ Documentation (Sphinx)
5. [ ] Interrupt handling (PLIC/CLINT)
6. [ ] Memory management (physical and virtual)
7. [ ] Process management and scheduling
8. [ ] AI accelerator support
