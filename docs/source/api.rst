API Reference
=============

UART Driver API
---------------

.. c:function:: void uart_init(void)

   Initialize the UART controller.
   
   Currently does nothing as OpenSBI pre-initializes the UART.
   
   :Example:
   
   .. code-block:: c
   
      void kernel_main(void) {
          uart_init();
          // UART ready to use
      }

.. c:function:: void uart_putc(char c)

   Write a single character to the UART.
   
   :param c: Character to transmit
   
   **Blocking:** Yes, waits for transmitter to be ready.
   
   :Example:
   
   .. code-block:: c
   
      uart_putc('H');
      uart_putc('i');
      uart_putc('\\n');

.. c:function:: void uart_puts(const char *s)

   Write a null-terminated string to the UART.
   
   :param s: Null-terminated string to transmit
   
   **Note:** Automatically converts ``\\n`` to ``\\r\\n`` for proper
   terminal display.
   
   **Blocking:** Yes, waits for each character to transmit.
   
   :Example:
   
   .. code-block:: c
   
      uart_puts("Hello, world!\\n");
      uart_puts("[OK] System initialized\\n");

.. c:function:: char uart_getc(void)

   Read a single character from the UART.
   
   :return: Character received from UART
   :rtype: char
   
   **Blocking:** Yes, waits indefinitely for input.
   
   :Example:
   
   .. code-block:: c
   
      uart_puts("Press any key: ");
      char c = uart_getc();
      uart_putc(c);

System Functions
----------------

.. c:function:: void kernel_main(void)

   Main entry point of the kernel.
   
   Called by bootloader after initialization. This function should
   never return.
   
   **Current implementation:**
   
   * Initialize UART
   * Print boot messages
   * Enter idle loop
   
   :Example:
   
   .. code-block:: c
   
      void kernel_main(void) {
          uart_init();
          uart_puts("ThunderOS booting...\\n");
          
          // Initialize subsystems
          // ...
          
          // Idle loop
          while (1) {
              asm volatile("wfi");
          }
      }

Assembly Functions
------------------

.. asm:label:: _start

   Kernel entry point from bootloader.
   
   **Location:** ``boot/boot.S``
   
   **Responsibilities:**
   
   1. Disable interrupts
   2. Setup stack pointer
   3. Clear BSS section
   4. Call ``kernel_main()``
   
   **Registers modified:**
   
   * ``sp`` - Set to ``_stack_top``
   * ``t0``, ``t1`` - Temporary for BSS clearing
   
   **Jumps to:** ``kernel_main`` (never returns)

Linker Symbols
--------------

These symbols are defined by the linker script and accessible from C:

.. c:var:: extern char _text_start[]

   Start address of ``.text`` section (executable code).

.. c:var:: extern char _text_end[]

   End address of ``.text`` section.

.. c:var:: extern char _rodata_start[]

   Start address of ``.rodata`` section (read-only data).

.. c:var:: extern char _rodata_end[]

   End address of ``.rodata`` section.

.. c:var:: extern char _data_start[]

   Start address of ``.data`` section (initialized data).

.. c:var:: extern char _data_end[]

   End address of ``.data`` section.

.. c:var:: extern char _bss_start[]

   Start address of ``.bss`` section (uninitialized data).

.. c:var:: extern char _bss_end[]

   End address of ``.bss`` section.

.. c:var:: extern char _kernel_end[]

   End address of entire kernel (page-aligned).
   
   Free memory begins at this address.

**Usage Example:**

.. code-block:: c

   extern char _bss_start[], _bss_end[], _kernel_end[];
   
   void print_memory_info(void) {
       size_t bss_size = _bss_end - _bss_start;
       uart_printf("BSS size: %zu bytes\\n", bss_size);
       uart_printf("Kernel ends at: %p\\n", _kernel_end);
   }

CSR Access Macros (Future)
---------------------------

.. c:macro:: read_csr(csr)

   Read a Control and Status Register.
   
   :param csr: CSR name (e.g., ``sstatus``, ``sie``)
   :return: Value of CSR
   
   **Example:**
   
   .. code-block:: c
   
      uint64_t status = read_csr(sstatus);

.. c:macro:: write_csr(csr, value)

   Write to a Control and Status Register.
   
   :param csr: CSR name
   :param value: Value to write
   
   **Example:**
   
   .. code-block:: c
   
      write_csr(sie, 0x222);  // Enable interrupts

.. c:macro:: set_csr(csr, bits)

   Set bits in a CSR (bitwise OR).
   
   :param csr: CSR name
   :param bits: Bits to set
   
   **Example:**
   
   .. code-block:: c
   
      set_csr(sstatus, 0x2);  // Enable supervisor interrupts

.. c:macro:: clear_csr(csr, bits)

   Clear bits in a CSR (bitwise AND NOT).
   
   :param csr: CSR name
   :param bits: Bits to clear
   
   **Example:**
   
   .. code-block:: c
   
      clear_csr(sstatus, 0x2);  // Disable supervisor interrupts

Memory Management API (Future)
-------------------------------

.. c:function:: void *kmalloc(size_t size)

   Allocate kernel memory.
   
   :param size: Number of bytes to allocate
   :return: Pointer to allocated memory, or NULL on failure
   :rtype: void*
   
   **Example:**
   
   .. code-block:: c
   
      char *buffer = kmalloc(1024);
      if (buffer) {
          // Use buffer
          kfree(buffer);
      }

.. c:function:: void kfree(void *ptr)

   Free kernel memory.
   
   :param ptr: Pointer to memory allocated by ``kmalloc()``
   
   **Example:**
   
   .. code-block:: c
   
      void *data = kmalloc(256);
      // ... use data ...
      kfree(data);

.. c:function:: void *kmalloc_aligned(size_t size, size_t align)

   Allocate aligned kernel memory.
   
   :param size: Number of bytes to allocate
   :param align: Alignment requirement (must be power of 2)
   :return: Pointer to aligned memory, or NULL on failure
   :rtype: void*
   
   **Example:**
   
   .. code-block:: c
   
      // Allocate page-aligned memory
      void *page = kmalloc_aligned(4096, 4096);

Process API (Future)
---------------------

.. c:function:: int process_create(void (*entry)(void))

   Create a new process.
   
   :param entry: Entry point function
   :return: Process ID, or negative on error
   :rtype: int
   
   **Example:**
   
   .. code-block:: c
   
      void user_task(void) {
          while (1) {
              uart_puts("Task running\\n");
              yield();
          }
      }
      
      int pid = process_create(user_task);

.. c:function:: void yield(void)

   Yield CPU to scheduler.
   
   **Example:**
   
   .. code-block:: c
   
      while (1) {
          do_work();
          yield();  // Let other processes run
      }

.. c:function:: void sleep(uint64_t milliseconds)

   Sleep for specified time.
   
   :param milliseconds: Time to sleep in milliseconds
   
   **Example:**
   
   .. code-block:: c
   
      uart_puts("Waiting...\\n");
      sleep(1000);  // Sleep 1 second
      uart_puts("Done!\\n");

Interrupt API (Future)
-----------------------

.. c:function:: void register_interrupt_handler(int irq, void (*handler)(void))

   Register interrupt handler.
   
   :param irq: Interrupt number
   :param handler: Handler function
   
   **Example:**
   
   .. code-block:: c
   
      void timer_interrupt(void) {
          uart_puts("Tick!\\n");
      }
      
      register_interrupt_handler(IRQ_TIMER, timer_interrupt);

.. c:function:: void enable_interrupts(void)

   Enable interrupts globally.
   
   **Example:**
   
   .. code-block:: c
   
      setup_interrupt_handlers();
      enable_interrupts();

.. c:function:: void disable_interrupts(void)

   Disable interrupts globally.
   
   **Example:**
   
   .. code-block:: c
   
      disable_interrupts();
      critical_section();
      enable_interrupts();

Constants
---------

Memory Addresses
~~~~~~~~~~~~~~~~

.. c:macro:: UART0_BASE

   Base address of UART0: ``0x10000000``

.. c:macro:: CLINT_BASE

   Base address of CLINT: ``0x02000000``

.. c:macro:: PLIC_BASE

   Base address of PLIC: ``0x0C000000``

.. c:macro:: RAM_START

   Start of RAM: ``0x80000000``

.. c:macro:: KERNEL_BASE

   Kernel load address: ``0x80200000``

Sizes
~~~~~

.. c:macro:: PAGE_SIZE

   Page size: ``4096`` bytes

.. c:macro:: STACK_SIZE

   Kernel stack size: ``16384`` bytes (16 KB)

Error Codes (Future)
~~~~~~~~~~~~~~~~~~~~

.. c:macro:: E_OK

   Success: ``0``

.. c:macro:: E_NOMEM

   Out of memory: ``-1``

.. c:macro:: E_INVAL

   Invalid argument: ``-2``

.. c:macro:: E_AGAIN

   Try again: ``-3``

Data Structures (Future)
-------------------------

.. c:type:: struct process

   Process control block.
   
   .. c:member:: int pid
   
      Process ID
   
   .. c:member:: void *stack
   
      Stack pointer
   
   .. c:member:: int state
   
      Process state (RUNNING, READY, BLOCKED, etc.)

.. c:type:: struct page

   Physical page descriptor.
   
   .. c:member:: uint64_t flags
   
      Page flags (USED, DIRTY, etc.)
   
   .. c:member:: int ref_count
   
      Reference count

See Also
--------

* :doc:`internals/uart_driver` - UART implementation details
* :doc:`internals/bootloader` - Bootloader internals
* :doc:`development` - Development guide
