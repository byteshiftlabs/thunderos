ThunderOS Documentation
=======================

**ThunderOS** is a lightweight, educational RISC-V operating system designed 
for learning OS development and embedded systems experimentation. It provides
a clean, well-documented foundation for understanding how operating systems work.

**License**: GNU General Public License v3.0 (GPL v3)

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   introduction
   architecture
   internals/index
   riscv/index
   development
   api

Project Goals
-------------

* Educational OS implementation for RISC-V
* Clean, readable, well-documented codebase
* Practical understanding of OS concepts
* Support for standard RISC-V extensions
* Foundation for further experimentation

Quick Start
-----------

Build the kernel::

    make all

Run in QEMU::

    make qemu

Current Status
--------------

**Version 0.9.0 - "Synchronization"** ✅ RELEASED

ThunderOS v0.9.0 includes blocking I/O, synchronization primitives, and 62 system calls!

.. list-table::
   :header-rows: 1

   * - Component
     - Status
   * - Bootloader
     - ✓ Implemented
   * - UART Driver
     - ✓ Implemented
   * - Trap Handler
     - ✓ Implemented
   * - Timer Interrupts (CLINT)
     - ✓ Implemented
   * - Memory Management (PMM + kmalloc)
     - ✓ Implemented
   * - Virtual Memory (Sv39 paging)
     - ✓ Implemented
   * - Process Scheduler
     - ✓ Implemented
   * - User Mode (U-mode) Support
     - ✓ Implemented
   * - System Calls (62 syscalls)
     - ✓ Implemented
   * - Privilege Separation
     - ✓ Implemented
   * - Exception Handling
     - ✓ Implemented
   * - VirtIO Block Device Driver
     - ✓ Implemented
   * - ext2 Filesystem Support
     - ✓ Implemented
   * - ELF Binary Loader
     - ✓ Implemented
   * - Signal Handling (POSIX-style)
     - ✓ Implemented
   * - Process Memory Isolation
     - ✓ Implemented
   * - Pipes and IPC
     - ✓ Implemented
   * - Synchronization Primitives
     - ✓ Implemented
   * - Automated Testing Framework
     - ✓ Implemented
   * - CI/CD Pipeline (GitHub Actions)
     - ✓ Implemented

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

License
=======

ThunderOS is free software licensed under the **GNU General Public License v3.0 (GPL v3)**.

This means you are free to:

* Use the software for any purpose
* Study how it works and modify it
* Distribute copies
* Distribute modified versions

Under the following conditions:

* Source code must be made available when distributing the software
* Modified versions must also be licensed under GPL v3
* Changes must be documented

See the full LICENSE file in the repository for complete terms and conditions.

**Copyright © 2025 ThunderOS Team**
