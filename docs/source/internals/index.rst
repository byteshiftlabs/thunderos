Kernel Internals
================

This section documents the internal implementation details of ThunderOS.

.. toctree::
   :maxdepth: 1
   :hidden:

   bootloader
   linker_script
   trap_handler
   interrupt_handling
   syscalls
   sbi
   pmm
   kmalloc
   paging
   memory
   dma
   barrier
   process_management
   user_mode
   shell
   signals
   pipes
   vfs
   ext2_filesystem
   elf_loader
   uart_driver
   hal_timer
   virtio_block
   virtio_gpu
   virtual_terminals
   hal/index
   kstring
   errno
   testing_framework

Component Reference
-------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Category
     - Components
   * - **Boot & Core**
     - :doc:`bootloader` · :doc:`linker_script` · :doc:`trap_handler` · :doc:`interrupt_handling` · :doc:`syscalls` · :doc:`sbi`
   * - **Memory**
     - :doc:`pmm` · :doc:`kmalloc` · :doc:`paging` · :doc:`memory` · :doc:`dma` · :doc:`barrier`
   * - **Processes**
     - :doc:`process_management` · :doc:`user_mode` · :doc:`shell` · :doc:`signals` · :doc:`pipes`
   * - **Filesystems**
     - :doc:`vfs` · :doc:`ext2_filesystem` · :doc:`elf_loader`
   * - **Drivers**
     - :doc:`uart_driver` · :doc:`hal_timer` · :doc:`virtio_block` · :doc:`virtio_gpu` · :doc:`virtual_terminals`
   * - **Utilities**
     - :doc:`kstring` · :doc:`errno` · :doc:`testing_framework`

Overview
--------

ThunderOS is implemented in a combination of:

* **RISC-V Assembly**: Bootloader and low-level initialization
* **C**: Kernel core and drivers
* **Linker Scripts**: Memory layout definition

The following pages provide detailed technical documentation of each component.

Code Organization
-----------------

Source Files
~~~~~~~~~~~~

.. code-block:: text

   boot/
   ├── entry.S             # M-mode entry point
   ├── start.c             # M-mode initialization
   └── boot.S              # S-mode bootloader

   kernel/
   ├── main.c              # Kernel entry point
   ├── arch/riscv64/
   │   ├── kernel.ld       # Linker script
   │   ├── trap_entry.S    # Assembly trap vector
   │   ├── switch.S        # Context switch
   │   ├── enter_usermode.S
   │   ├── user_return.S
   │   ├── core/
   │   │   └── trap.c      # C trap handler
   │   └── drivers/
   │       ├── uart.c      # UART driver
   │       ├── timer.c     # Timer driver
   │       ├── clint.c     # CLINT (timer/IPI)
   │       ├── plic.c      # PLIC (external IRQs)
   │       └── interrupt.c # Interrupt management
   ├── core/
   │   ├── panic.c         # Kernel panic handler
   │   ├── process.c       # Process management
   │   ├── scheduler.c     # Process scheduler
   │   ├── syscall.c       # System call handler
   │   ├── signal.c        # Signal handling
   │   ├── pipe.c          # Pipe IPC
   │   ├── shell.c         # Kernel shell
   │   ├── elf_loader.c    # ELF binary loader
   │   ├── errno.c         # Error handling
   │   ├── mutex.c         # Mutex implementation
   │   ├── condvar.c       # Condition variables
   │   ├── rwlock.c        # Read-write locks
   │   ├── wait_queue.c    # Wait queues
   │   └── time.c          # Time management
   ├── fs/
   │   ├── vfs.c           # Virtual filesystem
   │   ├── ext2_super.c    # ext2 superblock
   │   ├── ext2_inode.c    # ext2 inodes
   │   ├── ext2_dir.c      # ext2 directories
   │   ├── ext2_file.c     # ext2 file operations
   │   ├── ext2_alloc.c    # ext2 block allocation
   │   ├── ext2_write.c    # ext2 write support
   │   └── ext2_vfs.c      # ext2 VFS integration
   ├── drivers/
   │   ├── virtio_blk.c    # VirtIO block device
   │   ├── virtio_gpu.c    # VirtIO GPU
   │   ├── framebuffer.c   # Framebuffer driver
   │   ├── fbconsole.c     # Framebuffer console
   │   ├── vterm.c         # Virtual terminals
   │   └── font.c          # Console font
   ├── mm/
   │   ├── pmm.c           # Physical memory manager
   │   ├── kmalloc.c       # Kernel heap allocator
   │   ├── paging.c        # Virtual memory (Sv39)
   │   └── dma.c           # DMA allocator
   └── utils/
       └── kstring.c       # String utilities

   external/
   └── userland/            # User-space programs
       ├── bin/             # Shell commands (ls, cat, etc.)
       ├── core/            # C runtime (crt0, syscalls)
       ├── lib/             # User libraries
       └── tests/           # User-mode tests

See individual component pages for detailed technical documentation.
