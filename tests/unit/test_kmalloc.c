/*
 * kmalloc Unit Tests
 *
 * Tests kmalloc_aligned alignment correctness.
 * Directly verifies the B4 fix: aligned allocs must return properly-aligned
 * addresses, not page_base + HEADER_SIZE.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "mm/kmalloc.h"
#include "kernel/kstring.h"

#define TEST_PASS(name) do { \
    hal_uart_puts("  [PASS] " name "\n"); \
    tests_passed++; \
    tests_total++; \
} while (0)

#define TEST_FAIL(name, msg) do { \
    hal_uart_puts("  [FAIL] " name ": " msg "\n"); \
    tests_total++; \
} while (0)

#define ASSERT(name, cond, msg) do { \
    if (cond) { TEST_PASS(name); } else { TEST_FAIL(name, msg); } \
} while (0)

void test_kmalloc(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  kmalloc Unit Tests\n");
    hal_uart_puts("========================================\n\n");

    int tests_passed = 0;
    int tests_total = 0;

    /* Test 1: Basic kmalloc returns non-NULL */
    void *p = kmalloc(64);
    ASSERT("kmalloc(64) returns non-NULL", p != NULL,
           "kmalloc returned NULL for small allocation");
    if (p) kfree(p);

    /* Test 2: kmalloc_aligned(size, 16) — 16-byte alignment */
    void *a16 = kmalloc_aligned(64, 16);
    ASSERT("kmalloc_aligned returns non-NULL", a16 != NULL,
           "kmalloc_aligned returned NULL");
    ASSERT("kmalloc_aligned(64, 16) is 16-byte aligned",
           a16 != NULL && ((uintptr_t)a16 & 15) == 0,
           "address not 16-byte aligned");
    if (a16) kfree(a16);

    /* Test 3: kmalloc_aligned(size, 64) — 64-byte (cache line) alignment */
    void *a64 = kmalloc_aligned(128, 64);
    ASSERT("kmalloc_aligned(128, 64) returns non-NULL", a64 != NULL,
           "kmalloc_aligned returned NULL");
    ASSERT("kmalloc_aligned(128, 64) is 64-byte aligned",
           a64 != NULL && ((uintptr_t)a64 & 63) == 0,
           "address not 64-byte aligned");
    if (a64) kfree(a64);

    /* Test 4: kmalloc_aligned(size, 4096) — page alignment (the broken case) */
    void *a4096 = kmalloc_aligned(4096, 4096);
    ASSERT("kmalloc_aligned(4096, 4096) returns non-NULL", a4096 != NULL,
           "kmalloc_aligned returned NULL for page-aligned alloc");
    ASSERT("kmalloc_aligned(4096, 4096) is page-aligned",
           a4096 != NULL && ((uintptr_t)a4096 & 0xFFF) == 0,
           "address not page-aligned — B4 regression");
    if (a4096) kfree(a4096);

    /* Test 5: Two aligned allocs at same alignment return different addresses */
    void *b1 = kmalloc_aligned(64, 64);
    void *b2 = kmalloc_aligned(64, 64);
    ASSERT("two aligned allocs return distinct addresses",
           b1 != NULL && b2 != NULL && b1 != b2,
           "kmalloc_aligned returned duplicate addresses");
    if (b1) kfree(b1);
    if (b2) kfree(b2);

    /* Test 6: Aligned memory is writable */
    void *wr = kmalloc_aligned(256, 16);
    if (wr) {
        /* Write and read back a sentinel value */
        volatile unsigned char *buf = (volatile unsigned char *)wr;
        buf[0] = 0xAB;
        buf[255] = 0xCD;
        ASSERT("aligned memory is writable and readable",
               buf[0] == 0xAB && buf[255] == 0xCD,
               "read back wrong values");
        kfree(wr);
    } else {
        TEST_FAIL("aligned memory is writable and readable",
                  "kmalloc_aligned returned NULL");
    }

    hal_uart_puts("\n");
    hal_uart_puts("kmalloc tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n\n");
}

#endif /* ENABLE_KERNEL_TESTS */
