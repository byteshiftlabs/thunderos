/*
 * KUnit test runner implementation
 */

#include "kunit.h"
#include "uart.h"

// Helper to print integers
static void print_int(int val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    
    if (val < 0) {
        uart_putc('-');
        val = -val;
    }
    
    char buf[12];
    int i = 0;
    
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

// Run all test cases
int kunit_run_tests(struct kunit_test *test_cases, int num_tests) {
    int passed = 0;
    int failed = 0;
    
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  KUnit Test Suite - ThunderOS\n");
    uart_puts("========================================\n\n");
    
    // Run each test
    for (int i = 0; i < num_tests; i++) {
        struct kunit_test *test = &test_cases[i];
        
        // Reset test status
        test->status = TEST_SUCCESS;
        test->failure_msg = NULL;
        test->line = 0;
        
        // Run the test
        uart_puts("[ RUN      ] ");
        uart_puts(test->name);
        uart_puts("\n");
        
        test->run(test);
        
        // Check result
        if (test->status == TEST_SUCCESS) {
            uart_puts("[       OK ] ");
            uart_puts(test->name);
            uart_puts("\n");
            passed++;
        } else {
            uart_puts("[  FAILED  ] ");
            uart_puts(test->name);
            uart_puts("\n");
            uart_puts("             ");
            uart_puts(test->failure_msg);
            uart_puts(" at line ");
            print_int(test->line);
            uart_puts("\n");
            failed++;
        }
    }
    
    // Print summary
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("  Test Summary\n");
    uart_puts("========================================\n");
    uart_puts("Total:  ");
    print_int(num_tests);
    uart_puts("\n");
    uart_puts("Passed: ");
    print_int(passed);
    uart_puts("\n");
    uart_puts("Failed: ");
    print_int(failed);
    uart_puts("\n");
    
    if (failed == 0) {
        uart_puts("\nALL TESTS PASSED\n");
    } else {
        uart_puts("\nSOME TESTS FAILED\n");
    }
    
    uart_puts("========================================\n\n");
    
    return failed;
}
