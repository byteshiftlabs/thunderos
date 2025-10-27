Memory Layout
=============

This page documents the physical memory layout of ThunderOS as it runs on
the QEMU ``virt`` machine.

QEMU Virt Machine Memory Map
-----------------------------

The RISC-V ``virt`` machine in QEMU has the following memory-mapped devices:

.. code-block:: text

   ┌──────────────────┬─────────────┬────────────────────────────┐
   │ Address Range    │ Size        │ Device                     │
   ├──────────────────┼─────────────┼────────────────────────────┤
   │ 0x00000000       │ 256B        │ Test device                │
   │ 0x00001000       │ 4KB         │ Boot ROM (reset vector)    │
   │ 0x00100000       │ 4KB         │ RTC (Real Time Clock)      │
   │ 0x02000000       │ 64KB        │ CLINT (timer interrupts)   │
   │ 0x0C000000       │ 4MB         │ PLIC (interrupt controller)│
   │ 0x10000000       │ 256B        │ UART0 (serial console)     │
   │ 0x10001000       │ 4KB         │ VirtIO (disk, network, etc)│
   │ 0x40000000       │ 1GB         │ PCIe MMIO (future)         │
   │ 0x80000000       │ 128MB       │ **RAM** (our kernel here)  │
   └──────────────────┴─────────────┴────────────────────────────┘

RAM Organization (0x80000000 - 0x87FFFFFF)
-------------------------------------------

.. code-block:: text

   0x80000000  ┌─────────────────────────────────┐
               │ OpenSBI (M-mode firmware)       │
               │                                 │
               │ - SBI call handlers             │
               │ - Timer setup                   │
               │ - Console I/O                   │
               │                                 │
   0x80020000  ├─────────────────────────────────┤
               │                                 │
               │ (Reserved / unused)             │
               │                                 │
               │                                 │
   0x80200000  ├─────────────────────────────────┤ ← Kernel base
               │ ThunderOS Kernel                │
               │                                 │
               │ ┌─────────────────────────────┐ │
               │ │ .text.boot (bootloader)     │ │ ← _start
               │ ├─────────────────────────────┤ │
               │ │ .text (kernel code)         │ │
               │ │ - kernel_main()             │ │
               │ │ - uart_*() functions        │ │
               │ │ - (future: scheduler, mm)   │ │
               │ ├─────────────────────────────┤ │
               │ │ .rodata (const strings)     │ │
               │ │ - "ThunderOS"               │ │
               │ │ - "[OK] UART initialized"   │ │
               │ ├─────────────────────────────┤ │
               │ │ .data (initialized vars)    │ │
               │ │ (currently empty)           │ │
               │ ├─────────────────────────────┤ │
               │ │ .bss (uninitialized)        │ │
               │ │ - _stack (16KB)             │ │
               │ │ - (future: global vars)     │ │
               │ └─────────────────────────────┘ │
   ~0x80207000 ├─────────────────────────────────┤ ← _kernel_end
               │                                 │
               │ Free RAM                        │
               │                                 │
               │ (Future: heap, page tables,     │
               │  process memory, buffers, etc)  │
               │                                 │
               │                                 │
   0x87000000  ├─────────────────────────────────┤ ← Device tree blob (DTB)
               │ Firmware data structures        │
   0x87FFFFFF  └─────────────────────────────────┘

Kernel Memory Sections
----------------------

Based on linker script ``kernel/arch/riscv64/kernel.ld``:

.text.boot Section
~~~~~~~~~~~~~~~~~~

**Address:** 0x80200000 (exactly)

**Size:** ~256 bytes

**Contents:**
   * Entry point (``_start``)
   * Bootloader assembly code
   * Must be first for OpenSBI to jump to correctly

**Key Code:**

.. code-block:: asm

   0x80200000:  csrw sie, zero      # Disable interrupts
   0x80200004:  la sp, _stack_top   # Setup stack
   ...

.text Section
~~~~~~~~~~~~~

**Address:** 0x80200100 (approximately, after .text.boot)

**Size:** ~few KB (grows with more kernel code)

**Contents:**
   * ``kernel_main()``
   * ``uart_init()``, ``uart_putc()``, etc.
   * All C functions

**Example Functions:**

.. code-block:: text

   0x80200100:  kernel_main
   0x80200200:  uart_init
   0x80200210:  uart_putc
   0x80200250:  uart_puts
   0x80200290:  uart_getc

.rodata Section
~~~~~~~~~~~~~~~

**Address:** After .text, aligned

**Size:** ~1KB (string literals)

**Contents:**
   * String literals from ``uart_puts()`` calls
   * Const global variables
   * Read-only data

**Example Data:**

.. code-block:: text

   "ThunderOS - RISC-V AI OS\0"
   "[OK] UART initialized\0"
   "[  ] Interrupts: TODO\0"

.data Section
~~~~~~~~~~~~~

**Address:** After .rodata, aligned

**Size:** Currently 0 bytes (empty)

**Future Contents:**
   * Initialized global variables
   * Static variables with initializers

**Example (future):**

.. code-block:: c

   int process_count = 0;        // Goes in .data
   static char log_buffer[1024] = {};  // Goes in .data

.bss Section
~~~~~~~~~~~~

**Address:** After .data, aligned

**Size:** ~16KB (mostly stack)

**Contents:**
   * Kernel stack (16KB)
   * Uninitialized globals/statics

**Layout:**

.. code-block:: text

   _bss_start:
   0x80203000:  _stack_bottom
                [16384 bytes of stack space]
   0x80207000:  _stack_top (sp initially points here)
   _bss_end:

Stack Details
~~~~~~~~~~~~~

**Size:** 16KB (16384 bytes)

**Direction:** Grows downward (from _stack_top to _stack_bottom)

**Usage:**

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Stack Use
     - Approximate Size
   * - Function call overhead
     - 16-32 bytes per call
   * - Local variables
     - Varies (depends on function)
   * - Saved registers
     - 8 bytes × # of registers saved
   * - Function arguments (beyond a0-a7)
     - 8 bytes per extra arg

**Maximum Depth:**
   * With 16KB stack:
   * ~500 nested function calls (if each uses 32 bytes)
   * Deep recursion may overflow

Free Memory
-----------

**Start:** ``_kernel_end`` (symbol from linker script)

**End:** 0x87000000 (before firmware data)

**Size:** ~120MB (approximately)

**Future Uses:**
   * Kernel heap (kmalloc)
   * Page tables (virtual memory)
   * Process memory
   * Buffers (network, disk, etc.)
   * DMA regions

Memory Protection
-----------------

Current State
~~~~~~~~~~~~~

**No protection** - all memory accessible

* Kernel runs in S-mode (supervisor)
* No MMU enabled
* Physical addresses = virtual addresses
* Any bug can corrupt any memory

Future: Virtual Memory
~~~~~~~~~~~~~~~~~~~~~~

With paging enabled:

.. code-block:: text

   Virtual Address Space:
   
   0xFFFFFFFF_FFFFFFFF  ┌──────────────┐
                        │  Kernel      │
                        │  (direct map)│
   0xFFFFFFFF_80000000  ├──────────────┤
                        │              │
                        │   (hole)     │
                        │              │
   0x00000000_80000000  ├──────────────┤
                        │  User space  │
                        │  (processes) │
   0x00000000_00000000  └──────────────┘

Access Patterns
---------------

Read-Only Regions
~~~~~~~~~~~~~~~~~

* ``.text`` - Code (should be executable, not writable)
* ``.rodata`` - Constants (read-only)

**Future:** Mark as read-only in page tables

Read-Write Regions
~~~~~~~~~~~~~~~~~~

* ``.data`` - Initialized globals
* ``.bss`` - Uninitialized (stack, etc.)
* Free RAM - Dynamic allocations

Memory-Mapped I/O
-----------------

UART0 Registers
~~~~~~~~~~~~~~~

**Base:** 0x10000000

**Size:** 256 bytes

**Access:** 8-bit reads/writes

**Example:**

.. code-block:: c

   #define UART0_BASE 0x10000000
   #define UART_THR (UART0_BASE + 0)
   
   *(volatile uint8_t*)UART_THR = 'A';  // Send character

CLINT (Core Local Interruptor)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Base:** 0x02000000

**Size:** 64KB

**Purpose:** Timer interrupts, software interrupts

**Key Registers:**

.. code-block:: c

   #define CLINT_BASE 0x02000000
   #define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x4000 + (hart)*8)
   #define CLINT_MTIME (CLINT_BASE + 0xBFF8)

PLIC (Platform-Level Interrupt Controller)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Base:** 0x0C000000

**Size:** 4MB

**Purpose:** External interrupt routing

**Future use:** Route UART interrupts to kernel

Checking Memory Usage
---------------------

At Build Time
~~~~~~~~~~~~~

.. code-block:: bash

   # Section sizes
   riscv64-unknown-elf-size build/thunderos.elf
   
   # Output:
   #    text    data     bss     dec     hex filename
   #    1234      56   16384   17674    4502 thunderos.elf

At Runtime
~~~~~~~~~~

.. code-block:: c

   extern char _text_start[], _text_end[];
   extern char _rodata_start[], _rodata_end[];
   extern char _data_start[], _data_end[];
   extern char _bss_start[], _bss_end[];
   extern char _kernel_end[];

   void print_memory_map(void) {
       uart_printf("Kernel sections:\n");
       uart_printf("  .text:   %p - %p (%zu bytes)\n", 
                   _text_start, _text_end, 
                   _text_end - _text_start);
       uart_printf("  .rodata: %p - %p (%zu bytes)\n",
                   _rodata_start, _rodata_end,
                   _rodata_end - _rodata_start);
       uart_printf("  .data:   %p - %p (%zu bytes)\n",
                   _data_start, _data_end,
                   _data_end - _data_start);
       uart_printf("  .bss:    %p - %p (%zu bytes)\n",
                   _bss_start, _bss_end,
                   _bss_end - _bss_start);
       uart_printf("Kernel ends at: %p\n", _kernel_end);
       uart_printf("Free RAM: %zu MB\n",
                   (0x87000000 - (size_t)_kernel_end) / 1024 / 1024);
   }

Memory Alignment
----------------

Common alignment requirements:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Data Type
     - Alignment
     - Reason
   * - char
     - 1 byte
     - No requirement
   * - short
     - 2 bytes
     - Hardware efficiency
   * - int
     - 4 bytes
     - Hardware efficiency
   * - long/pointer
     - 8 bytes
     - RISC-V 64-bit natural alignment
   * - Page tables
     - 4096 bytes
     - Page size requirement
   * - Stack frames
     - 16 bytes
     - RISC-V ABI requirement

**RISC-V Note:** Unaligned access supported but slower (or trapped, depending on implementation).

See Also
--------

* :doc:`linker_script` - How layout is defined
* :doc:`bootloader` - Memory initialization
* :doc:`../architecture` - Overall system design
