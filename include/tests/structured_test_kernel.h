#ifndef TESTS_STRUCTURED_TEST_KERNEL_H
#define TESTS_STRUCTURED_TEST_KERNEL_H

#include "hal/hal_uart.h"
#include "kernel/kstring.h"

typedef struct {
    const char *suite_name;
    int passed;
    int failed;
    int skipped;
    int total;
} ktest_suite_t;

static inline void ktest_suite_init(ktest_suite_t *suite, const char *suite_name) {
    suite->suite_name = suite_name;
    suite->passed = 0;
    suite->failed = 0;
    suite->skipped = 0;
    suite->total = 0;
}

static inline void ktest_print_test_name(const ktest_suite_t *suite, const char *test_name) {
    hal_uart_puts(suite->suite_name);
    hal_uart_putc('.');
    hal_uart_puts(test_name);
}

static inline void ktest_suite_begin(const ktest_suite_t *suite) {
    hal_uart_puts("\n[----------] Tests from ");
    hal_uart_puts(suite->suite_name);
    hal_uart_puts("\n");
}

static inline void ktest_case_begin(const ktest_suite_t *suite, const char *test_name) {
    hal_uart_puts("[ RUN      ] ");
    ktest_print_test_name(suite, test_name);
    hal_uart_puts("\n");
}

static inline void ktest_case_pass(ktest_suite_t *suite, const char *test_name) {
    suite->passed++;
    suite->total++;
    hal_uart_puts("[       OK ] ");
    ktest_print_test_name(suite, test_name);
    hal_uart_puts("\n");
}

static inline void ktest_case_fail(ktest_suite_t *suite, const char *test_name, const char *message) {
    suite->failed++;
    suite->total++;
    if (message && message[0] != '\0') {
        hal_uart_puts("  ");
        hal_uart_puts(message);
        hal_uart_puts("\n");
    }
    hal_uart_puts("[  FAILED  ] ");
    ktest_print_test_name(suite, test_name);
    hal_uart_puts("\n");
}

static inline void ktest_case_skip(ktest_suite_t *suite, const char *test_name, const char *message) {
    suite->skipped++;
    suite->total++;
    if (message && message[0] != '\0') {
        hal_uart_puts("  ");
        hal_uart_puts(message);
        hal_uart_puts("\n");
    }
    hal_uart_puts("[  SKIPPED ] ");
    ktest_print_test_name(suite, test_name);
    hal_uart_puts("\n");
}

static inline void ktest_suite_end(const ktest_suite_t *suite) {
    hal_uart_puts("[----------] ");
    kprint_dec((size_t)suite->total);
    hal_uart_puts(" test(s) from ");
    hal_uart_puts(suite->suite_name);
    hal_uart_puts("\n");
    hal_uart_puts("[  PASSED  ] ");
    kprint_dec((size_t)suite->passed);
    hal_uart_puts(" test(s).\n");
    if (suite->skipped > 0) {
        hal_uart_puts("[  SKIPPED ] ");
        kprint_dec((size_t)suite->skipped);
        hal_uart_puts(" test(s).\n");
    }
    if (suite->failed > 0) {
        hal_uart_puts("[  FAILED  ] ");
        kprint_dec((size_t)suite->failed);
        hal_uart_puts(" test(s), listed from ");
        hal_uart_puts(suite->suite_name);
        hal_uart_puts(".\n");
    }
}

#define KTEST_ASSERT(suite, name, cond, message) \
    do { \
        ktest_case_begin(&(suite), (name)); \
        if (cond) { \
            ktest_case_pass(&(suite), (name)); \
        } else { \
            ktest_case_fail(&(suite), (name), (message)); \
        } \
    } while (0)

#endif