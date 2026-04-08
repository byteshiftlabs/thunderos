Testing Framework
=================

ThunderOS uses a KUnit-inspired testing framework for automated kernel testing. The framework provides assertion macros, test organization, and formatted output similar to Linux's KUnit.

Overview
--------

The testing framework enables:

* **Automated Testing**: Run tests in QEMU with no manual interaction
* **Regression Detection**: Catch bugs introduced by new changes
* **Documentation**: Tests serve as executable examples
* **Confidence**: Verify kernel components work correctly

Design Goals
~~~~~~~~~~~~

* **Lightweight**: Minimal overhead, suitable for bare-metal kernel
* **No Dependencies**: No libc, no standard library
* **Clear Output**: TAP-style formatting for easy parsing
* **Simple API**: Easy to write new tests

Architecture
------------

Components
~~~~~~~~~~

.. code-block:: text

   tests/framework/kunit.h      - Test macros and declarations
   tests/framework/kunit.c      - Test runner implementation
   tests/test_trap.c            - Trap handler tests
   tests/test_timer.c           - Timer interrupt tests
   tests/Makefile               - Build system for test kernels

Each test file is built into a **separate test kernel** that boots, runs tests, and prints results.

Test Kernel Structure
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   Test Kernel Boot
        |
        v
   [bootloader] (boot.S)
        |
        v
   [test_main()] (test_*.c)
        |
        ├─> uart_init()
        ├─> trap_init()
        ├─> clint_init() (if needed)
        |
        v
   [kunit_run_tests(test_suite)]
        |
        ├─> Print banner
        ├─> For each test:
        │    ├─> Print "[ RUN      ] test_name"
        │    ├─> Call test function
        │    └─> Print "[       OK ] test_name" or "[  FAILED  ] test_name"
        |
        v
   [Print Summary]
        |
        ├─> Total tests
        ├─> Passed tests
        ├─> Failed tests
        └─> Return status
        |
        v
   [Idle Loop - WFI]

API Reference
-------------

Test Definition
~~~~~~~~~~~~~~~

Define a test case using ``KUNIT_CASE`` macro:

.. code-block:: c

    static void test_example(struct kunit_test *test) {
         KUNIT_EXPECT_EQ(test, 2 + 2, 4);
         KUNIT_EXPECT_NE(test, 1, 0);
         KUNIT_EXPECT_TRUE(test, 1 == 1);
   }
   
    static struct kunit_test example_tests[] = {
       KUNIT_CASE(test_example),
       KUNIT_CASE(test_another),
   };

The ``KUNIT_CASE`` macro creates a ``struct kunit_test`` entry:

.. literalinclude:: ../../../tests/framework/kunit.h
    :language: c
    :lines: 29-29

Assertion Macros
~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Macro
     - Description
   * - ``KUNIT_EXPECT_EQ(test, a, b)``
     - Assert that ``a == b`` for the current ``struct kunit_test``
   * - ``KUNIT_EXPECT_NE(test, a, b)``
     - Assert that ``a != b`` for the current ``struct kunit_test``
   * - ``KUNIT_EXPECT_TRUE(test, cond)``
     - Assert that ``cond`` is true
   * - ``KUNIT_EXPECT_FALSE(test, cond)``
     - Assert that ``cond`` is false
   * - ``KUNIT_EXPECT_NULL(test, ptr)``
     - Assert that ``ptr == NULL``
   * - ``KUNIT_EXPECT_NOT_NULL(test, ptr)``
     - Assert that ``ptr != NULL``

Implementation
~~~~~~~~~~~~~~

.. literalinclude:: ../../../tests/framework/kunit.h
   :language: c
   :lines: 32-39

Each assertion checks the condition and:

* If **true**: Does nothing and the test continues
* If **false**: Stores a failure message and line number in the current test case, then returns immediately

The test returns immediately on a failed expectation, recording the failure message and source line in the current ``struct kunit_test`` entry.

Test Suite Structure
--------------------

Defining a Suite
~~~~~~~~~~~~~~~~

.. code-block:: c

   // tests/test_trap.c
   
   #include "kunit.h"
   #include "hal/hal_uart.h"
   #include "trap.h"
   
   // Individual test functions
   static void test_trap_initialized(struct kunit_test *test) {
       unsigned long stvec;
       asm volatile("csrr %0, stvec" : "=r"(stvec));
      KUNIT_EXPECT_NE(test, stvec, 0);
   }
   
   static void test_basic_arithmetic(struct kunit_test *test) {
       int result = 2 + 2;
      KUNIT_EXPECT_EQ(test, result, 4);
   }
   
   // Test case array
   static struct kunit_test trap_tests[] = {
       KUNIT_CASE(test_trap_initialized),
       KUNIT_CASE(test_basic_arithmetic),
   };
   
   // Main function
   void test_main(void) {
       uart_init();
       trap_init();
       
       uart_puts("\n=================================\n");
       uart_puts("   ThunderOS - Test Kernel\n");
       uart_puts("   Trap Handler Tests\n");
       uart_puts("=================================\n\n");
       
      int result = kunit_run_tests(trap_tests, 2);
       
       if (result == 0) {
           uart_puts("\nAll trap tests passed!\n");
       } else {
           uart_puts("\nSome trap tests failed!\n");
       }
       
       while (1) {
           asm volatile("wfi");
       }
   }

Test Runner
~~~~~~~~~~~

The ``kunit_run_tests()`` function iterates through test cases:

.. literalinclude:: ../../../tests/framework/kunit.c
   :language: c
   :lines: 28-92

Data Structures
---------------

Test Case Structure
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // tests/framework/kunit.h
   
      struct kunit_test {
       const char *name;           // Test function name (for display)
         void (*run)(struct kunit_test *test);  // Test function pointer
         enum test_status status;
         const char *failure_msg;
         int line;
   };

Example usage:

.. code-block:: c

      static struct kunit_test my_tests[] = {
       { .name = "test_addition", .run = test_addition },
       { .name = "test_subtraction", .run = test_subtraction },
   };

The ``KUNIT_CASE`` macro simplifies this:

.. code-block:: c

   KUNIT_CASE(test_addition)
   // Expands to:
   { .name = "test_addition", .run = test_addition }

Build System
------------


Writing Tests
-------------


Example: Testing Timer
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   // tests/test_timer.c (actual)
   
      static void test_timer_tick_increments(struct kunit_test *test) {
         hal_uart_puts("Waiting for timer interrupt (1 second)...\n");
       
       uint64_t start_ticks = clint_get_ticks();
       uint64_t timeout = read_time() + 15000000;
       
       // Wait for interrupt
       while (clint_get_ticks() == start_ticks) {
           asm volatile("wfi");
           
           if (read_time() > timeout) {
               hal_uart_puts("ERROR: Timer interrupt did not fire!\n");
               KUNIT_EXPECT_NE(test, clint_get_ticks(), start_ticks);
               return;
           }
       }
       
       // Print tick count
         hal_uart_puts("Tick: ");
       print_decimal(clint_get_ticks());
         hal_uart_puts("\n");
       
         KUNIT_EXPECT_EQ(test, clint_get_ticks(), start_ticks + 1);
   }


Test Output Format
------------------

Standard Output
~~~~~~~~~~~~~~~

When tests run, they produce formatted output:

.. code-block:: text

   ========================================
     KUnit Test Suite - ThunderOS
   ========================================
   
   [ RUN      ] test_trap_initialized
   [       OK ] test_trap_initialized
   [ RUN      ] test_trap_handler_installed
   [       OK ] test_trap_handler_installed
   [ RUN      ] test_basic_arithmetic
   [       OK ] test_basic_arithmetic
   [ RUN      ] test_pointer_operations
   [       OK ] test_pointer_operations
   
   ========================================
     Test Summary
   ========================================
   Total:  4
   Passed: 4
   Failed: 0
   
   ALL TESTS PASSED
   ========================================

Failure Output
~~~~~~~~~~~~~~

When a test fails:

.. code-block:: text

   [ RUN      ] test_division
     FAIL: Expected result == 2
   [  FAILED  ] test_division

The test continues after failure, so multiple assertions can be checked.

TAP Format (Future)
~~~~~~~~~~~~~~~~~~~

For automated parsing, we could output TAP (Test Anything Protocol):

.. code-block:: text

   TAP version 13
   1..4
   ok 1 - test_trap_initialized
   ok 2 - test_trap_handler_installed
   ok 3 - test_basic_arithmetic
   ok 4 - test_pointer_operations

Best Practices
--------------

Test Organization
~~~~~~~~~~~~~~~~~

* **One file per component**: ``test_uart.c``, ``test_timer.c``, ``test_memory.c``
* **Descriptive names**: ``test_timer_interrupts_enabled`` not ``test1``
* **Small tests**: Each test should verify one thing
* **Fast tests**: Avoid long delays unless testing timing

Test Isolation
~~~~~~~~~~~~~~

* **No dependencies**: Tests should not depend on execution order
* **Clean state**: Each test starts fresh (separate kernel boot)
* **No side effects**: Tests shouldn't affect other tests

Assertion Guidelines
~~~~~~~~~~~~~~~~~~~~

* **Use specific assertions**: ``KUNIT_EXPECT_EQ(x, 5)`` not ``KUNIT_EXPECT_TRUE(x == 5)``
* **Check positive and negative**: Test both success and failure paths
* **Meaningful messages**: Assertion failures should be self-explanatory

Documentation
~~~~~~~~~~~~~

* **Comment tricky tests**: Explain why something is tested
* **Use tests as examples**: Tests show how to use the API
* **Keep tests up-to-date**: Update when implementation changes

Running Tests
-------------

Build All Tests
~~~~~~~~~~~~~~~

.. code-block:: bash

   cd tests
   make clean
   make

This builds:

* ``build/tests/test_trap.elf``
* ``build/tests/test_timer.elf``
* (future test kernels)

Run Individual Test
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   make run-test-trap
   make run-test-timer

Run All Tests
~~~~~~~~~~~~~

.. code-block:: bash

   make test  # (Future: run all test targets)

Automated Testing
~~~~~~~~~~~~~~~~~

Create a script to run all tests and check results:

.. code-block:: bash

   #!/bin/bash
   # run_all_tests.sh
   
   FAILED=0
   
   for test in tests/run-test-*; do
       echo "Running $test..."
       if ! make -C tests $test > /tmp/test.log 2>&1; then
           echo "FAILED: $test"
           FAILED=$((FAILED + 1))
       fi
   done
   
   if [ $FAILED -eq 0 ]; then
       echo "All tests passed!"
       exit 0
   else
       echo "$FAILED test(s) failed"
       exit 1
   fi

Debugging Tests
---------------

Debug with GDB
~~~~~~~~~~~~~~

.. code-block:: bash

   # Start QEMU with GDB server
   qemu-system-riscv64 -machine virt -m 128M -nographic \
       -serial mon:stdio -bios default \
       -kernel build/tests/test_trap.elf \
       -s -S
   
   # In another terminal
   riscv64-unknown-elf-gdb build/tests/test_trap.elf
   (gdb) target remote :1234
   (gdb) break test_trap_initialized
   (gdb) continue

Add Debug Prints
~~~~~~~~~~~~~~~~

.. code-block:: c

   static void test_complex_operation(void) {
       uart_puts("DEBUG: Starting test\n");
       
       int result = complex_calculation();
       uart_puts("DEBUG: result = ");
       print_decimal(result);
       uart_puts("\n");
       
       KUNIT_EXPECT_EQ(result, 42);
   }

Check CSR Values
~~~~~~~~~~~~~~~~

.. code-block:: c

   static void debug_test_state(void) {
       unsigned long sstatus, sie;
       asm volatile("csrr %0, sstatus" : "=r"(sstatus));
       asm volatile("csrr %0, sie" : "=r"(sie));
       
       uart_puts("sstatus: "); print_hex(sstatus); uart_puts("\n");
       uart_puts("sie:     "); print_hex(sie); uart_puts("\n");
   }

Existing Test Suites
--------------------

Trap Handler Tests
~~~~~~~~~~~~~~~~~~

**File**: ``tests/test_trap.c``

**Tests**: 4

* ``test_trap_initialized``: Verify stvec is set
* ``test_trap_handler_installed``: Check trap handler is installed
* ``test_basic_arithmetic``: Basic math works (no exceptions)
* ``test_pointer_operations``: Memory access works

Timer Tests
~~~~~~~~~~~

**File**: ``tests/test_timer.c``

**Tests**: 6

* ``test_timer_interrupts_enabled``: Check sie.STIE bit
* ``test_global_interrupts_enabled``: Check sstatus.SIE bit
* ``test_initial_ticks_zero``: Tick counter starts at zero
* ``test_rdtime_works``: rdtime instruction advances
* ``test_timer_tick_increments``: Wait for interrupt, check tick++
* ``test_multiple_ticks``: Wait for multiple interrupts

Future Enhancements
-------------------

Planned improvements:

1. **More Assertion Types**
   
   * ``KUNIT_EXPECT_GT``, ``KUNIT_EXPECT_LT`` (greater/less than)
   * ``KUNIT_EXPECT_STREQ`` (string comparison)
   * ``KUNIT_EXPECT_NEAR`` (floating point comparison)

2. **Test Fixtures**
   
   * Setup/teardown functions
   * Shared test state
   * Resource management

3. **Parameterized Tests**
   
   * Run same test with different inputs
   * Table-driven tests
   * Property-based testing

4. **Coverage Reporting**
   
   * Track which code is tested
   * Identify untested paths
   * Coverage visualization

5. **Performance Benchmarks**
   
   * Measure execution time
   * Compare implementations
   * Regression detection

6. **Mocking Support**
   
   * Mock hardware interfaces
   * Simulate error conditions
   * Test edge cases

7. **TAP Output**
   
   * Standard test output format
   * CI/CD integration
   * Automated result parsing

References
----------

* **KUnit Documentation**: Linux kernel testing framework
* **TAP Specification**: Test Anything Protocol
* **xUnit Architecture**: Test framework patterns

See Also
--------

* :doc:`trap_handler` - Component tested by test_trap.c
* :doc:`hal_timer` - Component tested by timer-focused checks
* :doc:`../development` - Development workflow and testing practices
