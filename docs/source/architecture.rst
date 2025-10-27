System Architecture
===================

ThunderOS follows a monolithic kernel architecture with modular components.

High-Level Overview
-------------------

.. code-block:: text

                    ┌─────────────────────────────┐
                    │    User Space (Future)      │
                    └─────────────────────────────┘
                               │ syscalls
                    ┌──────────▼──────────────────┐
                    │      Kernel Space           │
                    │  ┌──────────────────────┐   │
                    │  │   Process Scheduler  │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │  Memory Management   │   │
                    │  └──────────────────────┘   │
                    │  ┌──────────────────────┐   │
                    │  │   Device Drivers     │   │
                    │  │   - UART             │   │
                    │  │   - PLIC/CLINT       │   │
                    │  │   - AI Accelerators  │   │
                    │  └──────────────────────┘   │
                    └─────────────────────────────┘
                               │
                    ┌──────────▼──────────────────┐
                    │     Hardware (QEMU)         │
                    │  - RISC-V 64-bit CPU        │
                    │  - Memory (128MB)           │
                    │  - UART (NS16550A)          │
                    │  - PLIC/CLINT               │
                    └─────────────────────────────┘

Boot Process
------------

The boot sequence follows this flow:

1. **Power On / Reset**
   
   * CPU starts at reset vector (implementation-dependent)
   * In QEMU virt machine: 0x1000

2. **Firmware Stage (OpenSBI)**
   
   * OpenSBI loads at 0x80000000
   * Initializes hardware (UART, timers, interrupts)
   * Provides SBI (Supervisor Binary Interface) services
   * Runs in M-mode (Machine mode - highest privilege)

3. **Bootloader (boot.S)**
   
   * Loads at 0x80200000
   * Entry point: ``_start``
   * Runs in S-mode (Supervisor mode)
   * Responsibilities:
   
     * Disable interrupts
     * Setup stack pointer
     * Clear BSS section
     * Jump to C kernel

4. **Kernel Initialization (kernel_main)**
   
   * Initialize UART
   * Print boot messages
   * (Future) Setup interrupts, memory, processes
   * Enter idle loop

Memory Layout
-------------

ThunderOS uses the following memory map on QEMU virt machine:

.. code-block:: text

   ┌─────────────────────┬──────────────┬─────────────────────┐
   │ Address Range       │ Size         │ Description         │
   ├─────────────────────┼──────────────┼─────────────────────┤
   │ 0x00001000          │ 4KB          │ Boot ROM            │
   │ 0x02000000          │ 16KB         │ CLINT (timer)       │
   │ 0x0C000000          │ 32MB         │ PLIC (interrupts)   │
   │ 0x10000000          │ 256B         │ UART0               │
   │ 0x80000000          │ 128KB        │ OpenSBI (firmware)  │
   │ 0x80200000          │ ~1MB         │ ThunderOS Kernel    │
   │   ├─ .text          │              │   Code segment      │
   │   ├─ .rodata        │              │   Read-only data    │
   │   ├─ .data          │              │   Initialized data  │
   │   └─ .bss           │              │   Uninitialized     │
   │ 0x87000000          │ ~120MB       │ Free RAM            │
   └─────────────────────┴──────────────┴─────────────────────┘

Privilege Levels
----------------

RISC-V defines multiple privilege levels:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Level
     - Name
     - Usage
   * - 0 (U)
     - User
     - Application code (future)
   * - 1 (S)
     - Supervisor
     - **ThunderOS kernel runs here**
   * - 3 (M)
     - Machine
     - OpenSBI firmware

ThunderOS runs in **S-mode** and relies on OpenSBI (M-mode) for:

* Timer interrupts via SBI calls
* Console I/O (early boot)
* System reset/shutdown

Key Components
--------------

Bootloader
~~~~~~~~~~

* **File**: ``boot/boot.S``
* **Purpose**: First code executed after firmware
* **Language**: RISC-V assembly
* Responsibilities:

  * Environment setup (stack, BSS)
  * Transfer control to C code

UART Driver
~~~~~~~~~~~

* **Files**: ``kernel/drivers/uart.c``, ``include/uart.h``
* **Purpose**: Serial console I/O
* **Hardware**: NS16550A compatible UART
* Functions:

  * ``uart_init()`` - Initialize driver
  * ``uart_putc()`` - Output character
  * ``uart_puts()`` - Output string
  * ``uart_getc()`` - Input character

Kernel Main
~~~~~~~~~~~

* **File**: ``kernel/main.c``
* **Purpose**: Main kernel entry point
* **Current functionality**:

  * Initialize UART
  * Print boot messages
  * Idle loop (WFI)

Build System
------------

The build process uses GNU Make:

.. code-block:: make

   # Toolchain
   CC = riscv64-unknown-elf-gcc
   LD = riscv64-unknown-elf-ld
   
   # Flags
   CFLAGS = -march=rv64gc -mabi=lp64d -ffreestanding
   LDFLAGS = -T kernel/arch/riscv64/kernel.ld

Build targets:

* ``make all`` - Build kernel ELF and binary
* ``make clean`` - Remove build artifacts
* ``make qemu`` - Run kernel in QEMU
* ``make debug`` - Run with GDB server
* ``make dump`` - Generate disassembly

QEMU Configuration
------------------

ThunderOS targets the ``virt`` machine in QEMU:

.. code-block:: bash

   qemu-system-riscv64 \
     -machine virt \      # Generic virtual RISC-V board
     -m 128M \            # 128MB RAM
     -nographic \         # No GUI, use terminal
     -serial mon:stdio \  # Serial to stdout
     -bios default \      # Use built-in OpenSBI
     -kernel thunderos.elf

The ``virt`` machine provides:

* 1 RISC-V CPU (configurable)
* RAM at 0x80000000
* UART at 0x10000000
* PLIC at 0x0C000000
* CLINT at 0x02000000
* VirtIO devices (for future use)

Future Architecture
-------------------

Planned components:

**Interrupt Handling**
   * PLIC driver for external interrupts
   * CLINT driver for timer interrupts
   * Trap handlers for exceptions

**Memory Management**
   * Physical memory allocator
   * Virtual memory (paging)
   * Kernel heap

**Process Management**
   * Process/task structures
   * Context switching
   * Scheduler (AI-aware)

**AI Acceleration**
   * Vector instruction support (RVV)
   * Accelerator drivers
   * DMA management

See :doc:`internals/index` for detailed implementation documentation.
