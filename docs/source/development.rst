Development Guide
=================

Contributing to ThunderOS
-------------------------

Code Style
~~~~~~~~~~

**C Code:**

* Use K&R style bracing
* 4 spaces indentation (no tabs)
* Descriptive variable names
* Comment non-obvious code

.. code-block:: c

   // Good
   void init_memory_manager(void) {
       size_t total_pages = calculate_available_pages();
       for (size_t i = 0; i < total_pages; i++) {
           mark_page_free(i);
       }
   }

   // Bad
   void imm(){
   int tp=cap();for(int i=0;i<tp;i++){mpf(i);}}

**Assembly:**

* Comment each logical block
* Use meaningful labels
* Align code for readability

.. code-block:: asm

   # Good
   clear_bss:
       beq t0, t1, clear_bss_done    # Exit if done
       sd zero, 0(t0)                # Zero current word
       addi t0, t0, 8                # Advance pointer
       j clear_bss                   # Loop
   clear_bss_done:

Building
--------

Prerequisites
~~~~~~~~~~~~~

.. code-block:: bash

   # RISC-V toolchain
   apt-get install gcc-riscv64-unknown-elf
   
   # QEMU
   apt-get install qemu-system-riscv64
   
   # Build tools
   apt-get install make

Compilation
~~~~~~~~~~~

.. code-block:: bash

   # Full build
   make all
   
   # Clean build
   make clean && make all
   
   # Run in QEMU
   make qemu
   
   # Debug with GDB
   make debug

Testing
-------

Manual Testing
~~~~~~~~~~~~~~

1. Build kernel: ``make all``
2. Run in QEMU: ``make qemu``
3. Verify boot messages appear
4. Test each feature manually

Automated Testing
~~~~~~~~~~~~~~~~~

Create test scripts:

.. code-block:: bash

   #!/bin/bash
   # tests/boot_test.sh
   
   timeout 5 make qemu > output.txt 2>&1
   
   if grep -q "ThunderOS" output.txt; then
       echo "PASS: Kernel boots"
   else
       echo "FAIL: Kernel doesn't boot"
       exit 1
   fi

Debugging
---------

QEMU + GDB
~~~~~~~~~~

Terminal 1:

.. code-block:: bash

   make debug
   # QEMU waits for GDB connection

Terminal 2:

.. code-block:: bash

   riscv64-unknown-elf-gdb build/thunderos.elf
   (gdb) target remote :1234
   (gdb) break kernel_main
   (gdb) continue

Useful GDB Commands
~~~~~~~~~~~~~~~~~~~

.. code-block:: gdb

   # Breakpoints
   break kernel_main
   break *0x80200000
   
   # Execution
   continue
   step          # Step into
   next          # Step over
   stepi         # Step one instruction
   
   # Inspection
   info registers
   x/10i $pc     # Disassemble
   x/20x $sp     # Examine stack
   backtrace     # Call stack
   
   # Watchpoints
   watch variable_name
   rwatch address   # Read watchpoint
   awatch address   # Access watchpoint

Common Issues
~~~~~~~~~~~~~

**Kernel doesn't boot:**

* Check entry point matches linker script
* Verify stack is set up correctly
* Use GDB to step through bootloader

**Random crashes:**

* Stack overflow
* Unaligned access
* Null pointer dereference

**UART not working:**

* Check base address (0x10000000)
* Verify volatile keyword on pointers
* Test with simpler string

Adding Features
---------------

New Driver
~~~~~~~~~~

1. Create driver files:

.. code-block:: bash

   # kernel/drivers/timer.c
   # include/timer.h

2. Implement interface:

.. code-block:: c

   // include/timer.h
   void timer_init(void);
   uint64_t timer_get_ticks(void);
   void timer_delay(uint64_t ms);

3. Add to Makefile:

.. code-block:: make

   KERNEL_C_SOURCES := ... $(wildcard $(KERNEL_DIR)/drivers/*.c)

4. Initialize in kernel_main:

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       timer_init();  // New driver
       ...
   }

New Subsystem
~~~~~~~~~~~~~

Example: Memory allocator

1. Create directory:

.. code-block:: bash

   mkdir kernel/mm

2. Implement:

.. code-block:: c

   // kernel/mm/pmm.c - Physical memory manager
   void *pmm_alloc_page(void);
   void pmm_free_page(void *page);

3. Header:

.. code-block:: c

   // include/mm.h
   void *kmalloc(size_t size);
   void kfree(void *ptr);

4. Integrate:

.. code-block:: c

   void kernel_main(void) {
       uart_init();
       pmm_init();
       kmalloc_init();
       ...
   }

Documentation
-------------

Writing Docs
~~~~~~~~~~~~

ThunderOS uses Sphinx with reStructuredText:

.. code-block:: rst

   New Feature
   ===========
   
   Overview
   --------
   
   This feature does XYZ.
   
   **Example:**
   
   .. code-block:: c
   
      int result = new_feature(42);

Building Docs
~~~~~~~~~~~~~

.. code-block:: bash

   cd docs
   make html
   
   # View in browser
   firefox build/html/index.html

Release Process
---------------

Version Numbering
~~~~~~~~~~~~~~~~~

* v0.x.y - Pre-release
* v1.0.0 - First stable
* vX.Y.Z - Semantic versioning

Checklist
~~~~~~~~~

1. Update version in code
2. Update CHANGELOG.md
3. Build and test thoroughly
4. Tag release: ``git tag v0.2.0``
5. Push: ``git push --tags``

Project Roadmap
---------------

Current (v0.1)
~~~~~~~~~~~~~~

* ✓ Bootloader
* ✓ UART driver
* ✓ Basic kernel main

Phase 1 (v0.2)
~~~~~~~~~~~~~~

* Interrupt handling (PLIC/CLINT)
* Timer driver
* Basic exception handlers

Phase 2 (v0.3)
~~~~~~~~~~~~~~

* Physical memory manager
* Virtual memory (paging)
* Kernel heap (kmalloc/kfree)

Phase 3 (v0.4)
~~~~~~~~~~~~~~

* Process structures
* Context switching
* Basic scheduler

Phase 4 (v0.5)
~~~~~~~~~~~~~~

* System calls
* User mode
* Simple shell

Future
~~~~~~

* Filesystems
* Network stack
* AI accelerator support
* Vector instruction optimization

Community
---------

Getting Help
~~~~~~~~~~~~

* Read the documentation first
* Check existing issues
* Ask in discussions

Reporting Bugs
~~~~~~~~~~~~~~

Include:

1. What you expected
2. What actually happened
3. Steps to reproduce
4. Your environment (QEMU version, etc.)

Feature Requests
~~~~~~~~~~~~~~~~

Open an issue with:

1. Use case
2. Proposed API
3. Implementation ideas (if any)

License
-------

ThunderOS is open source (specify license).

See Also
--------

* :doc:`architecture` - System design
* :doc:`internals/index` - Implementation details
* :doc:`api` - API reference
