/*
 * PMM (Physical Memory Manager) Unit Tests
 *
 * Tests pmm_alloc_page, pmm_free_page, and page accounting.
 * Verifies the locking fix (S1) didn't break correctness.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "mm/pmm.h"
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

void test_pmm(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  PMM Unit Tests\n");
    hal_uart_puts("========================================\n\n");

    int tests_passed = 0;
    int tests_total = 0;

    size_t total_before, free_before;
    pmm_get_stats(&total_before, &free_before);

    /* Test 1: Single page alloc returns non-zero */
    uintptr_t p1 = pmm_alloc_page();
    ASSERT("alloc returns non-zero", p1 != 0, "pmm_alloc_page returned 0");

    /* Test 2: Page is page-aligned */
    ASSERT("alloc is page-aligned", (p1 & 0xFFF) == 0,
           "returned address is not 4KB-aligned");

    /* Test 3: Second alloc returns a different page */
    uintptr_t p2 = pmm_alloc_page();
    ASSERT("two allocs return distinct pages", p1 != p2,
           "pmm_alloc_page returned same address twice");

    /* Test 4: Free count decreases by 2 after two allocs */
    size_t total_after, free_after;
    pmm_get_stats(&total_after, &free_after);
    ASSERT("free count decreases after alloc",
           free_before - free_after == 2,
           "free page count did not decrease by 2");

    /* Test 5: Free one page — free count recovers by 1 */
    pmm_free_page(p1);
    size_t total_r1, free_r1;
    pmm_get_stats(&total_r1, &free_r1);
    ASSERT("free count recovers after free",
           free_r1 == free_after + 1,
           "free page count did not increase after pmm_free_page");

    /* Test 6: Multi-page alloc returns non-zero */
    uintptr_t mp = pmm_alloc_pages(4);
    ASSERT("alloc_pages(4) returns non-zero", mp != 0,
           "pmm_alloc_pages returned 0");

    /* Test 7: Multi-page alloc is page-aligned */
    ASSERT("alloc_pages is page-aligned", (mp & 0xFFF) == 0,
           "pmm_alloc_pages returned unaligned address");

    /* Test 8: Free multi-page — free count recovers */
    size_t total_mp, free_mp;
    pmm_get_stats(&total_mp, &free_mp);
    pmm_free_pages(mp, 4);
    size_t total_rmp, free_rmp;
    pmm_get_stats(&total_rmp, &free_rmp);
    ASSERT("free count recovers after free_pages",
           free_rmp == free_mp + 4,
           "free count did not increase by 4 after pmm_free_pages");

    /* Clean up p2 */
    pmm_free_page(p2);

    /* Test 9: Final free count matches initial */
    size_t total_final, free_final;
    pmm_get_stats(&total_final, &free_final);
    ASSERT("free count restored to initial after all frees",
           free_final == free_before,
           "final free count does not match initial count");

    hal_uart_puts("\n");
    hal_uart_puts("PMM tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n\n");
}

#endif /* ENABLE_KERNEL_TESTS */
