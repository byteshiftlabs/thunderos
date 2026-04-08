Signal Handling
===============

Overview
--------

ThunderOS implements POSIX-style signals for inter-process communication and process control. Signals provide an asynchronous notification mechanism that allows processes to:

- Handle termination requests gracefully
- Receive notifications of child process state changes
- Implement custom signal handlers in user space
- Control process execution (stop, continue, terminate)

**Key Features:**

- Per-process signal masks (pending and blocked)
- User-defined signal handlers
- Default signal actions (terminate, ignore, stop, continue)
- Signal delivery via trap frame modification
- Integration with trap handler for user mode delivery

Signal Numbers
--------------

ThunderOS supports 32 signals (1-31), following POSIX conventions:

**Termination Signals:**

.. literalinclude:: ../../../include/kernel/signal.h
   :language: c
   :lines: 16-31

The same header defines user and process-control signals such as ``SIGUSR1``, ``SIGUSR2``, ``SIGCHLD``, ``SIGCONT``, ``SIGSTOP``, ``SIGTSTP``, ``SIGTTIN``, and ``SIGTTOU``.

**Special Properties:**

- ``SIGKILL`` and ``SIGSTOP`` cannot be caught, blocked, or ignored
- ``SIGCHLD`` is ignored by default
- All other termination signals terminate the process by default

Signal Handler Types
--------------------

ThunderOS defines three special signal handler values:

.. literalinclude:: ../../../include/kernel/signal.h
   :language: c
   :lines: 41-44

**Handler Behavior:**

- ``SIG_DFL``: Execute default action based on signal type
- ``SIG_IGN``: Ignore the signal completely
- User function pointer: Execute custom handler in user space

Data Structures
---------------

Process Control Block
~~~~~~~~~~~~~~~~~~~~~~

Each process has signal-related fields in its PCB:

.. literalinclude:: ../../../include/kernel/process.h
   :language: c
   :lines: 136-149

**Signal Set Type:**

.. literalinclude:: ../../../include/kernel/signal.h
   :language: c
   :lines: 46-50

Each bit represents one signal (bit N = signal N).

Signal Action Structure
~~~~~~~~~~~~~~~~~~~~~~~

For advanced signal handling (sigaction syscall):

.. literalinclude:: ../../../include/kernel/signal.h
   :language: c
   :lines: 52-57

**Flags (currently unused):**

.. literalinclude:: ../../../include/kernel/signal.h
   :language: c
   :lines: 59-63

Signal Delivery Mechanism
--------------------------

Signal delivery is a two-phase process:

Phase 1: Sending
~~~~~~~~~~~~~~~~

When a signal is sent via ``sys_kill()`` or ``signal_send()``:

1. **Validate signal number and target process**
2. **Set pending bit**: ``proc->pending_signals |= (1UL << signum)``
3. **Wake sleeping process** (except for SIGCONT)
4. **Return to caller** (signal not yet delivered)

.. literalinclude:: ../../../kernel/core/signal.c
   :language: c
   :lines: 91-121

Phase 2: Delivery
~~~~~~~~~~~~~~~~~

Signals are delivered when returning to user mode from a trap:

1. **Check for pending signals** in trap handler (before ``sret``)
2. **Find first unblocked signal**: ``deliverable = pending & ~blocked``
3. **Clear pending bit**: ``proc->pending_signals &= ~(1UL << signum)``
4. **Execute handler** (default action or user handler)
5. **Modify trap frame** if user handler (see below)

.. code-block:: c

   void trap_handler(struct trap_frame *tf) {
       unsigned long cause = read_scause();
       
       if (cause & INTERRUPT_BIT) {
           handle_interrupt(tf, cause);
       } else {
           handle_exception(tf, cause);
       }
       
       // Deliver pending signals before returning to user mode
       struct process *current = process_current();
       if (current) {
           signal_deliver_with_frame(current, tf);
       }
   }

**Delivery Function:**

.. literalinclude:: ../../../kernel/core/signal.c
   :language: c
   :lines: 381-407

User Handler Execution
-----------------------

When a user-defined signal handler is registered, ThunderOS modifies the trap frame to redirect execution:

Trap Frame Modification
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void signal_handle_with_frame(struct process *proc, int signum, 
                                   struct trap_frame *tf) {
       sighandler_t handler = proc->signal_handlers[signum];
       
       if (handler != SIG_DFL && handler != SIG_IGN) {
           // User-defined handler
           if (tf) {
               // Save return address: handler will 'ret' to interrupted code
               tf->ra = tf->sepc;
               
               // Redirect execution to signal handler
               tf->sepc = (unsigned long)handler;
               tf->a0 = signum;  // Pass signal number as argument
           }
       }
   }

**Execution Flow:**

1. **Process interrupted** by timer/syscall/exception
2. **Trap handler runs** in kernel mode
3. **Signal detected** and handler address retrieved
4. **Trap frame modified**:
   
   - ``tf->ra = tf->sepc`` (return address = interrupted PC)
   - ``tf->sepc = handler`` (new PC = signal handler)
   - ``tf->a0 = signum`` (argument = signal number)

5. **Return to user mode** via ``sret``
6. **Signal handler executes** at handler address
7. **Handler ends with ``ret``** instruction
8. **Execution resumes** at interrupted location (from ``ra``)

**Example Handler:**

.. code-block:: c

   // User-space signal handler
   void my_signal_handler(int signum) {
       // Handle signal (write message, set flag, etc.)
       signal_received = 1;
       
       // Handler automatically returns to interrupted code
       // No explicit return needed - 'ret' uses saved ra
   }

Limitations (Current Implementation)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The current implementation is simplified:

- **No signal frame**: Full context not saved on user stack
- **No sigreturn**: Can't restore full register state
- **Simple return**: Handler must end with ``ret`` instruction
- **No nested signals**: Only one signal delivered per trap

**Future Improvements:**

- Implement signal frame on user stack
- Add ``sys_sigreturn()`` for context restoration
- Support nested signal handlers
- Implement signal masks (block signals during handler)

Default Signal Actions
----------------------

When a signal handler is ``SIG_DFL``, the default action is executed:

Terminate
~~~~~~~~~

Signals that terminate the process by default:

.. code-block:: c

   void signal_default_term(struct process *proc) {
       process_exit(SIGNAL_EXIT_BASE);  // Exit code 128
   }

**Signals:** SIGKILL, SIGTERM, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGALRM, SIGHUP, SIGUSR1, SIGUSR2

Ignore
~~~~~~

Signals that are ignored by default:

.. code-block:: c

   void signal_default_ignore(struct process *proc) {
       // Do nothing
   }

**Signals:** SIGCHLD

Stop
~~~~

Signals that stop (suspend) the process:

.. code-block:: c

   void signal_default_stop(struct process *proc) {
       // Change state to SLEEPING (stopped)
       proc->state = PROC_SLEEPING;
       
       // Notify parent with SIGCHLD
       if (proc->parent) {
           signal_send(proc->parent, SIGCHLD);
       }
   }

**Signals:** SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU

Continue
~~~~~~~~

Signals that resume a stopped process:

.. code-block:: c

   void signal_default_cont(struct process *proc) {
       // If process was stopped, wake it up
       if (proc->state == PROC_SLEEPING) {
           process_wakeup(proc);
       }
   }

**Signals:** SIGCONT

System Calls
------------

sys_kill
~~~~~~~~

Send a signal to a process.

**Prototype:**

.. literalinclude:: ../../../include/kernel/syscall.h
   :language: c
   :lines: 142

**Parameters:**

- ``pid``: Target process ID
- ``signum``: Signal number to send

**Return Value:**

- ``0`` on success
- ``-1`` on error (sets errno)

**Errors:**

- ``THUNDEROS_EINVAL``: Invalid PID or signal number
- ``THUNDEROS_ESRCH``: No such process (process not found or zombie)

**Example:**

.. code-block:: c

   // User-space code
   int pid = getpid();
   kill(pid, SIGUSR1);  // Send SIGUSR1 to self

sys_signal
~~~~~~~~~~

Set a signal handler.

**Prototype:**

.. literalinclude:: ../../../include/kernel/syscall.h
   :language: c
   :lines: 152

**Parameters:**

- ``signum``: Signal number
- ``handler``: Handler function (``SIG_DFL``, ``SIG_IGN``, or user function)

**Return Value:**

- Previous handler on success
- ``SIG_ERR`` (``-1``) on error (sets errno)

**Errors:**

- ``THUNDEROS_EINVAL``: Invalid signal number or attempt to catch SIGKILL/SIGSTOP
- ``THUNDEROS_ESRCH``: No current process

**Example:**

.. code-block:: c

   // User-space code
   void my_handler(int sig) {
       // Handle signal
   }
   
   signal(SIGUSR1, my_handler);     // Install handler
   signal(SIGUSR2, SIG_IGN);        // Ignore signal
   signal(SIGTERM, SIG_DFL);        // Restore default

sys_sigaction (handler-only)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install or query a signal action. ThunderOS currently supports only the handler
field of ``struct sigaction``. Per-handler signal masks (``sa_mask``) and flags
(``sa_flags``) are not yet supported and must be zero; requests with non-zero
masks or flags return ``THUNDEROS_ENOSYS``.

**Prototype:**

.. literalinclude:: ../../../include/kernel/syscall.h
   :language: c
   :lines: 153

**Current Status:** Handler-only subset implemented. Validates user pointers,
copies the previous handler into ``oldact`` when provided, and installs new
handlers via ``signal_set_handler()``. Returns ``THUNDEROS_ENOSYS`` only when
``sa_mask != 0`` or ``sa_flags != 0``.

sys_sigreturn (not yet implemented)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Return from a signal handler. The current signal-delivery model uses a simple
return-to-``sepc`` path and does not yet provide a sigreturn trampoline or full
interrupted-context restoration ABI.

**Prototype:**

.. literalinclude:: ../../../include/kernel/syscall.h
   :language: c
   :lines: 154

**Current Status:** Returns ``THUNDEROS_ENOSYS``. Will be implemented when
full signal-frame save/restore is added.

Process Integration
-------------------

Signal Initialization
~~~~~~~~~~~~~~~~~~~~~

When a process is created, signals are initialized:

.. code-block:: c

   void signal_init_process(struct process *proc) {
       if (!proc) return;
       
       proc->pending_signals = 0;
       proc->blocked_signals = 0;
       
       // Set all handlers to default
       for (int i = 0; i < NSIG; i++) {
           proc->signal_handlers[i] = SIG_DFL;
       }
   }

This is called from ``process_create()`` and ``process_create_elf()``.

SIGCHLD on Exit
~~~~~~~~~~~~~~~

When a process exits, it sends ``SIGCHLD`` to its parent:

.. code-block:: c

   void process_exit(int exit_code) {
       struct process *proc = current_process;
       
       // Mark as zombie
       proc->state = PROC_ZOMBIE;
       proc->exit_code = exit_code;
       
       // Send SIGCHLD to parent
       if (proc->parent) {
           signal_send(proc->parent, SIGCHLD);
       }
       
       // Yield forever
       while (1) {
           process_yield();
       }
   }

This allows parent processes to be notified when children terminate.

Error Handling
--------------

All signal functions use the errno system for error reporting:

.. code-block:: c

   // Success path
   clear_errno();
   return 0;
   
   // Error path
   set_errno(THUNDEROS_EINVAL);
   return -1;

**Common Error Codes:**

- ``THUNDEROS_EINVAL`` (22): Invalid argument
- ``THUNDEROS_ESRCH`` (3): No such process
- ``THUNDEROS_ENOSYS`` (38): Function not implemented

Testing
-------

Signal Test Program
~~~~~~~~~~~~~~~~~~~

``external/userland/tests/signal_test.c`` validates signal functionality:

**Tests:**

1. Install SIGUSR1 handler
2. Send SIGUSR1 to self (verify delivery)
3. Install SIGUSR2 handler
4. Send SIGUSR2 to self (verify delivery)
5. Send multiple SIGUSR1 signals (verify count)

**Expected Output:**

.. code-block:: text

   === ThunderOS Signal Test ===
   
   [TEST 1] Installing SIGUSR1 handler...
     Handler installed
   [TEST 2] Sending SIGUSR1 to self...
     [SIGNAL] SIGUSR1 received!
     ✓ SIGUSR1 delivered successfully
   [TEST 3] Installing SIGUSR2 handler...
     Handler installed
   [TEST 4] Sending SIGUSR2 to self...
     [SIGNAL] SIGUSR2 received!
     ✓ SIGUSR2 delivered successfully
   [TEST 5] Sending multiple SIGUSR1 signals...
     [SIGNAL] SIGUSR1 received!
     [SIGNAL] SIGUSR1 received!
     [SIGNAL] SIGUSR1 received!
     Signal count: 3
   
   === Signal Test Complete ===
   All basic signal tests passed!

Implementation Files
--------------------

**Header:**

- ``include/kernel/signal.h`` - Signal API, constants, and prototypes

**Implementation:**

- ``kernel/core/signal.c`` - Signal handling logic (417 lines)

**Integration:**

- ``kernel/core/syscall.c`` - Signal syscalls
- ``kernel/core/process.c`` - Signal initialization, SIGCHLD
- ``kernel/arch/riscv64/core/trap.c`` - Signal delivery

**Test:**

- ``external/userland/tests/signal_test.c`` - User-space signal test program

Future Enhancements
-------------------

Planned improvements for signal handling:

1. **Signal Frame and sigreturn**
   
   - Save full register state on user stack
   - Implement ``sys_sigreturn()`` for proper restoration
   - Support nested signal handlers

2. **Signal Masking**
   
   - Implement ``sigprocmask()`` syscall
   - Block signals during handler execution
   - Support ``sa_mask`` in sigaction

3. **Advanced sigaction**
   
   - Full sigaction structure support
   - ``SA_RESTART`` flag for syscall restart
   - ``SA_SIGINFO`` for extended signal info

4. **Real-time Signals**
   
   - Add SIGRTMIN through SIGRTMAX (33-64)
   - Queued signals with data payloads
   - Priority-based delivery

5. **Process Groups**
   
   - Send signals to process groups
   - Job control (SIGTTIN, SIGTTOU, SIGTSTP)
   - Terminal-related signals

References
----------

- POSIX.1-2017 Signal Concepts
- Linux signal(7) man page
- RISC-V Privileged Specification (trap handling)
- ThunderOS errno system documentation (:doc:`errno`)
- ThunderOS trap handler documentation (:doc:`trap_handler`)
