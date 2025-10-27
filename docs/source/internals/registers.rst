RISC-V Registers Reference
==========================

This page documents the RISC-V registers used by ThunderOS.

General Purpose Registers
--------------------------

RISC-V has 32 integer registers, each 64 bits wide (in RV64).

Register File
~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 10 15 15 60

   * - Number
     - Name
     - ABI Name
     - Purpose
   * - x0
     - zero
     - zero
     - Hardwired to 0
   * - x1
     - ra
     - ra
     - Return address
   * - x2
     - sp
     - sp
     - Stack pointer
   * - x3
     - gp
     - gp
     - Global pointer (unused in ThunderOS)
   * - x4
     - tp
     - tp
     - Thread pointer (future use)
   * - x5-x7
     - t0-t2
     - t0-t2
     - Temporary registers
   * - x8
     - s0/fp
     - s0/fp
     - Saved register / frame pointer
   * - x9
     - s1
     - s1
     - Saved register
   * - x10-x11
     - a0-a1
     - a0-a1
     - Function arguments / return values
   * - x12-x17
     - a2-a7
     - a2-a7
     - Function arguments
   * - x18-x27
     - s2-s11
     - s2-s11
     - Saved registers
   * - x28-x31
     - t3-t6
     - t3-t6
     - Temporary registers

Register Conventions
~~~~~~~~~~~~~~~~~~~~

**Caller-Saved (Temporary)**
   * t0-t6, a0-a7
   * Function can clobber these
   * Caller must save if needed across calls

**Callee-Saved**
   * s0-s11, sp
   * Function must preserve these
   * Save on entry, restore on exit

**Special Registers**
   * zero (x0): Always 0, writes ignored
   * ra (x1): Return address for function calls
   * sp (x2): Stack pointer (must be 16-byte aligned per ABI)

Usage in ThunderOS
~~~~~~~~~~~~~~~~~~

**Bootloader (boot.S):**

.. code-block:: asm

   la sp, _stack_top     # sp = stack pointer
   la t0, _bss_start     # t0 = loop pointer
   la t1, _bss_end       # t1 = loop bound
   sd zero, 0(t0)        # zero = constant 0

**C Function Calls:**

.. code-block:: c

   int add(int a, int b) {  // a0=a, a1=b
       return a + b;         // return in a0
   }
   
   int result = add(5, 7);   // 5→a0, 7→a1, result←a0

Control and Status Registers (CSRs)
------------------------------------

CSRs are special registers for system configuration and control.

Supervisor CSRs (S-mode)
~~~~~~~~~~~~~~~~~~~~~~~~~

These are accessible in supervisor mode (where ThunderOS runs):

.. list-table::
   :header-rows: 1
   :widths: 15 15 70

   * - Address
     - Name
     - Description
   * - 0x100
     - sstatus
     - Supervisor status (interrupt enable, privilege, etc.)
   * - 0x104
     - sie
     - Supervisor interrupt enable
   * - 0x105
     - stvec
     - Supervisor trap handler base address
   * - 0x140
     - sscratch
     - Scratch register for trap handler
   * - 0x141
     - sepc
     - Supervisor exception program counter
   * - 0x142
     - scause
     - Supervisor trap cause
   * - 0x143
     - stval
     - Supervisor trap value (bad address, etc.)
   * - 0x144
     - sip
     - Supervisor interrupt pending
   * - 0x180
     - satp
     - Supervisor address translation and protection

CSR Instructions
~~~~~~~~~~~~~~~~

.. code-block:: asm

   csrr  rd, csr      # Read CSR to register
   csrw  csr, rs      # Write register to CSR
   csrs  csr, rs      # Set bits in CSR (OR)
   csrc  csr, rs      # Clear bits in CSR (AND NOT)
   csrrw rd, csr, rs  # Read-write (atomic swap)
   csrrs rd, csr, rs  # Read-set
   csrrc rd, csr, rs  # Read-clear

**Example:**

.. code-block:: asm

   # Disable interrupts
   csrw sie, zero
   
   # Enable interrupts
   li t0, 0x222        # Software, timer, external
   csrw sie, t0
   
   # Read exception PC
   csrr a0, sepc

sstatus - Supervisor Status
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Key bits:

.. code-block:: text

   Bit  Name  Description
   ───────────────────────────────────────
   0    UIE   User Interrupt Enable
   1    SIE   Supervisor Interrupt Enable
   5    SPIE  Previous SIE (before trap)
   8    SPP   Previous Privilege (0=U, 1=S)
   63   SD    State Dirty (FS/XS modified)

**Usage:**

.. code-block:: asm

   # Disable supervisor interrupts
   csrci sstatus, 0x2   # Clear SIE bit
   
   # Enable supervisor interrupts
   csrsi sstatus, 0x2   # Set SIE bit

sie - Supervisor Interrupt Enable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Bit  Name  Description
   ───────────────────────────────────────
   1    SSIE  Supervisor Software Interrupt Enable
   5    STIE  Supervisor Timer Interrupt Enable
   9    SEIE  Supervisor External Interrupt Enable

**Usage in bootloader:**

.. code-block:: asm

   # boot.S disables all interrupts
   csrw sie, zero

stvec - Supervisor Trap Vector
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Points to trap handler.

**Format:**

.. code-block:: text

   Bits[63:2]: BASE (handler address, 4-byte aligned)
   Bits[1:0]:  MODE
               00 = Direct (all traps to BASE)
               01 = Vectored (interrupts to BASE + 4*cause)

**Future usage:**

.. code-block:: c

   extern void trap_handler(void);
   
   void setup_traps(void) {
       // Set trap handler address (direct mode)
       write_csr(stvec, (uint64_t)trap_handler);
   }

scause - Supervisor Cause
~~~~~~~~~~~~~~~~~~~~~~~~~~

Indicates trap cause.

**Format:**

.. code-block:: text

   Bit 63: Interrupt (1) or Exception (0)
   Bits[62:0]: Exception/Interrupt Code

**Exception Codes (Interrupt=0):**

.. list-table::
   :widths: 15 85

   * - Code
     - Exception
   * - 0
     - Instruction address misaligned
   * - 1
     - Instruction access fault
   * - 2
     - Illegal instruction
   * - 3
     - Breakpoint
   * - 4
     - Load address misaligned
   * - 5
     - Load access fault
   * - 6
     - Store address misaligned
   * - 7
     - Store access fault
   * - 8
     - Environment call from U-mode
   * - 9
     - Environment call from S-mode
   * - 12
     - Instruction page fault
   * - 13
     - Load page fault
   * - 15
     - Store page fault

**Interrupt Codes (Interrupt=1):**

.. list-table::
   :widths: 15 85

   * - Code
     - Interrupt
   * - 1
     - Supervisor software interrupt
   * - 5
     - Supervisor timer interrupt
   * - 9
     - Supervisor external interrupt

stval - Supervisor Trap Value
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Additional trap information:

* Address that caused fault (for page faults)
* Instruction that caused exception (for illegal instruction)
* 0 for other traps

sepc - Supervisor Exception PC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Address of instruction that caused trap.

**Usage in handler:**

.. code-block:: c

   void trap_handler(void) {
       uint64_t cause = read_csr(scause);
       uint64_t epc = read_csr(sepc);
       uint64_t tval = read_csr(stval);
       
       if (cause & (1UL << 63)) {
           // Handle interrupt
       } else {
           // Handle exception
           uart_printf("Exception at %p\n", epc);
       }
       
       // Return to next instruction
       write_csr(sepc, epc + 4);
   }

satp - Supervisor Address Translation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Controls virtual memory (paging).

**Format (Sv39 mode):**

.. code-block:: text

   Bits[63:60]: MODE
                0 = Bare (no translation)
                8 = Sv39 (39-bit virtual address)
                9 = Sv48 (48-bit virtual address)
   Bits[59:44]: ASID (Address Space ID)
   Bits[43:0]:  PPN (Physical Page Number of page table)

**Future usage:**

.. code-block:: c

   // Enable Sv39 paging
   uint64_t satp = (8UL << 60) | (page_table_ppn & 0xFFFFFFFFFFF);
   write_csr(satp, satp);
   asm volatile("sfence.vma");  // Flush TLB

Floating-Point Registers
-------------------------

ThunderOS doesn't currently use floating-point, but RISC-V has:

* **f0-f31**: 32 floating-point registers (64-bit double precision)
* **fcsr**: Floating-point control and status

Vector Registers (RVV)
----------------------

RISC-V Vector Extension provides:

* **v0-v31**: 32 vector registers (length configurable)
* **vl**: Vector length
* **vtype**: Vector type configuration

**Future Use for AI:**

.. code-block:: asm

   # Matrix multiplication using vectors
   vsetvli t0, a0, e32, m1   # Configure vector length
   vle32.v v1, (a1)          # Load vector from memory
   vle32.v v2, (a2)          # Load vector from memory
   vfmacc.vv v0, v1, v2      # Fused multiply-accumulate
   vse32.v v0, (a3)          # Store result

Register Saving
---------------

Context Switch
~~~~~~~~~~~~~~

When switching between processes, save/restore:

.. code-block:: c

   struct context {
       uint64_t ra;       // Return address
       uint64_t sp;       // Stack pointer
       uint64_t s0-s11;   // Saved registers
       uint64_t sepc;     // Exception PC
       // ... CSRs as needed
   };
   
   void switch_to(struct context *old, struct context *new) {
       // Save old context
       // Restore new context
       // sret to return
   }

Trap Handler
~~~~~~~~~~~~

On trap entry, hardware saves:

* **sepc**: Current PC
* **scause**: Trap cause
* **stval**: Trap value
* **sstatus.SPIE**: Previous SIE state

Handler must save:

* All registers it uses
* Or: save all registers if preemptive

**Example:**

.. code-block:: asm

   trap_handler:
       # Save all registers
       addi sp, sp, -256
       sd ra, 0(sp)
       sd t0, 8(sp)
       sd t1, 16(sp)
       ...
       
       # Handle trap in C
       call trap_handler_c
       
       # Restore all registers
       ld ra, 0(sp)
       ld t0, 8(sp)
       ld t1, 16(sp)
       ...
       addi sp, sp, 256
       
       sret   # Return from trap

Register Debugging
------------------

GDB Commands
~~~~~~~~~~~~

.. code-block:: gdb

   info registers          # All general-purpose registers
   info all-registers      # Include CSRs
   print/x $sp            # Print stack pointer
   print/x $ra            # Print return address
   print/x $a0            # Print first argument

   # CSRs (may need special GDB build)
   monitor reg sstatus
   monitor reg sie

Printing in Kernel
~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void dump_registers(void) {
       uint64_t sp, ra;
       asm volatile("mv %0, sp" : "=r"(sp));
       asm volatile("mv %0, ra" : "=r"(ra));
       
       uart_printf("sp = %p\n", sp);
       uart_printf("ra = %p\n", ra);
       uart_printf("sstatus = %p\n", read_csr(sstatus));
       uart_printf("sie = %p\n", read_csr(sie));
   }

See Also
--------

* `RISC-V ISA Specification <https://riscv.org/technical/specifications/>`_
* `RISC-V Calling Convention <https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf>`_
* :doc:`bootloader` - Register usage in assembly
* :doc:`../architecture` - System overview
