/*
 * PMM (Physical Memory Manager) Unit Tests
 *
 * Tests pmm_alloc_page, pmm_free_page, and page accounting.
 * Verifies the locking fix (S1) didn't break correctness.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "mm/pmm.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

void test_pmm(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "PMM");
    ktest_suite_begin(&suite);

    size_t total_before, free_before;
    pmm_get_stats(&total_before, &free_before);

    uintptr_t p1 = pmm_alloc_page();
    ASSERT(suite, "AllocReturnsNonZero", p1 != 0, "pmm_alloc_page returned 0");

    ASSERT(suite, "AllocationIsPageAligned", (p1 & 0xFFF) == 0,
           "returned address is not 4KB-aligned");

    uintptr_t p2 = pmm_alloc_page();
    ASSERT(suite, "DistinctAllocationsReturnDifferentPages", p1 != p2,
           "pmm_alloc_page returned same address twice");

    size_t total_after, free_after;
    pmm_get_stats(&total_after, &free_after);
    ASSERT(suite, "FreeCountDropsAfterAllocation",
           free_before - free_after == 2,
           "free page count did not decrease by 2");

    pmm_free_page(p1);
    size_t total_r1, free_r1;
    pmm_get_stats(&total_r1, &free_r1);
    ASSERT(suite, "FreeCountRecoversAfterSingleFree",
           free_r1 == free_after + 1,
           "free page count did not increase after pmm_free_page");

    uintptr_t mp = pmm_alloc_pages(4);
    ASSERT(suite, "MultiPageAllocationReturnsNonZero", mp != 0,
           "pmm_alloc_pages returned 0");

    ASSERT(suite, "MultiPageAllocationIsPageAligned", (mp & 0xFFF) == 0,
           "pmm_alloc_pages returned unaligned address");

    size_t total_mp, free_mp;
    pmm_get_stats(&total_mp, &free_mp);
    pmm_free_pages(mp, 4);
    size_t total_rmp, free_rmp;
    pmm_get_stats(&total_rmp, &free_rmp);
    ASSERT(suite, "FreeCountRecoversAfterFreePages",
           free_rmp == free_mp + 4,
           "free count did not increase by 4 after pmm_free_pages");

    pmm_free_page(p2);

    size_t total_final, free_final;
    pmm_get_stats(&total_final, &free_final);
    ASSERT(suite, "FreeCountRestoredToInitialAfterAllFrees",
           free_final == free_before,
           "final free count does not match initial count");

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
