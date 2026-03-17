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

#include "mm/kmalloc.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

void test_kmalloc(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "Kmalloc");
    ktest_suite_begin(&suite);

    void *p = kmalloc(64);
    ASSERT(suite, "BasicAllocationReturnsNonNull", p != NULL,
           "kmalloc returned NULL for small allocation");
    if (p) kfree(p);

    void *a16 = kmalloc_aligned(64, 16);
    ASSERT(suite, "AlignedAllocationReturnsNonNull", a16 != NULL,
           "kmalloc_aligned returned NULL");
    ASSERT(suite, "SixteenByteAlignmentIsRespected",
           a16 != NULL && ((uintptr_t)a16 & 15) == 0,
           "address not 16-byte aligned");
    if (a16) kfree(a16);

    void *a64 = kmalloc_aligned(128, 64);
    ASSERT(suite, "CacheLineAlignedAllocationReturnsNonNull", a64 != NULL,
           "kmalloc_aligned returned NULL");
    ASSERT(suite, "CacheLineAlignmentIsRespected",
           a64 != NULL && ((uintptr_t)a64 & 63) == 0,
           "address not 64-byte aligned");
    if (a64) kfree(a64);

    void *a4096 = kmalloc_aligned(4096, 4096);
    ASSERT(suite, "PageAlignedAllocationReturnsNonNull", a4096 != NULL,
           "kmalloc_aligned returned NULL for page-aligned alloc");
    ASSERT(suite, "PageAlignedAllocationIsPageAligned",
           a4096 != NULL && ((uintptr_t)a4096 & 0xFFF) == 0,
           "address not page-aligned — B4 regression");
    if (a4096) kfree(a4096);

    void *b1 = kmalloc_aligned(64, 64);
    void *b2 = kmalloc_aligned(64, 64);
    ASSERT(suite, "DistinctAlignedAllocationsReturnDifferentAddresses",
           b1 != NULL && b2 != NULL && b1 != b2,
           "kmalloc_aligned returned duplicate addresses");
    if (b1) kfree(b1);
    if (b2) kfree(b2);

    void *wr = kmalloc_aligned(256, 16);
    if (wr) {
        volatile unsigned char *buf = (volatile unsigned char *)wr;
        buf[0] = 0xAB;
        buf[255] = 0xCD;
        ASSERT(suite, "AlignedMemoryIsWritableAndReadable",
               buf[0] == 0xAB && buf[255] == 0xCD,
               "read back wrong values");
        kfree(wr);
    } else {
        ASSERT(suite, "AlignedMemoryIsWritableAndReadable", 0,
               "kmalloc_aligned returned NULL");
    }

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
