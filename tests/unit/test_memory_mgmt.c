/*
 * Memory Management Test Program
 * 
 * Tests DMA allocation, address translation, and memory barriers
 * 
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "mm/dma.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "arch/barrier.h"
#include "tests/structured_test_kernel.h"

void test_memory_management(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "MemoryFeatures");
    ktest_suite_begin(&suite);

    dma_region_t *region1 = dma_alloc(8192, DMA_ZERO);
    KTEST_ASSERT(suite, "DmaAllocationReturnsNonNull",
                 region1 != NULL,
                 "dma_alloc(8192, DMA_ZERO) returned NULL");

    if (region1 != NULL) {
        uint8_t *ptr = (uint8_t *)dma_virt_addr(region1);
        int all_zero = 1;
        for (size_t i = 0; i < 256; i++) {
            if (ptr[i] != 0) {
                all_zero = 0;
                break;
            }
        }

        KTEST_ASSERT(suite, "DmaZeroFlagClearsInitialBytes",
                     all_zero,
                     "DMA_ZERO allocation was not fully zeroed in the first 256 bytes");
    } else {
        ktest_case_begin(&suite, "DmaZeroFlagClearsInitialBytes");
        ktest_case_skip(&suite, "DmaZeroFlagClearsInitialBytes", "region1 allocation failed");
    }

    if (region1 != NULL) {
        uintptr_t phys_base = dma_phys_addr(region1);
        uintptr_t virt_base = (uintptr_t)dma_virt_addr(region1);
        uintptr_t phys_expected = phys_base + PAGE_SIZE;
        uintptr_t phys_page2;
        int result = virt_to_phys(get_kernel_page_table(), virt_base + PAGE_SIZE, &phys_page2);

        KTEST_ASSERT(suite, "DmaRegionIsPhysicallyContiguous",
                     result == 0 && phys_page2 == phys_expected,
                     "translated second DMA page was not physically contiguous");
    } else {
        ktest_case_begin(&suite, "DmaRegionIsPhysicallyContiguous");
        ktest_case_skip(&suite, "DmaRegionIsPhysicallyContiguous", "region1 allocation failed");
    }

    dma_region_t *region2 = dma_alloc(4096, 0);
    KTEST_ASSERT(suite, "SecondDmaRegionAllocationReturnsNonNull",
                 region2 != NULL,
                 "dma_alloc(4096, 0) returned NULL");

    if (region1 != NULL) {
        uintptr_t virt = (uintptr_t)dma_virt_addr(region1);
        uintptr_t phys_expected = dma_phys_addr(region1);
        uintptr_t phys_translated = translate_virt_to_phys(virt);

        KTEST_ASSERT(suite, "VirtualToPhysicalTranslationMatchesDmaRegion",
                     phys_translated == phys_expected,
                     "translate_virt_to_phys did not return the DMA region physical address");
    } else {
        ktest_case_begin(&suite, "VirtualToPhysicalTranslationMatchesDmaRegion");
        ktest_case_skip(&suite, "VirtualToPhysicalTranslationMatchesDmaRegion", "region1 allocation failed");
    }

    memory_barrier();
    write_barrier();
    read_barrier();
    io_barrier();
    data_memory_barrier();
    data_sync_barrier();
    compiler_barrier();

    KTEST_ASSERT(suite, "BarrierInstructionsExecuteWithoutFault", 1,
                 "barrier helper sequence should complete without faulting");

    if (region1 != NULL) {
        volatile uint32_t *ptr = (volatile uint32_t *)dma_virt_addr(region1);
        write32_barrier(ptr, 0xDEADBEEF);
        uint32_t value = read32_barrier(ptr);

        KTEST_ASSERT(suite, "BarrierReadWriteHelpersRoundTripValue",
                     value == 0xDEADBEEF,
                     "read32_barrier did not return the value written with write32_barrier");
    } else {
        ktest_case_begin(&suite, "BarrierReadWriteHelpersRoundTripValue");
        ktest_case_skip(&suite, "BarrierReadWriteHelpersRoundTripValue", "region1 allocation failed");
    }

    size_t num_regions = 0;
    size_t num_bytes = 0;
    dma_get_stats(&num_regions, &num_bytes);
    size_t expected_regions = (region1 != NULL ? 1 : 0) + (region2 != NULL ? 1 : 0);
    size_t expected_bytes = (region1 != NULL ? 8192 : 0) + (region2 != NULL ? 4096 : 0);

    KTEST_ASSERT(suite, "DmaStatisticsMatchActiveAllocations",
                 num_regions == expected_regions && num_bytes == expected_bytes,
                 "DMA statistics did not match the active allocation set");

    if (region1 != NULL) {
        dma_free(region1);
        region1 = NULL;
        dma_get_stats(&num_regions, &num_bytes);

        KTEST_ASSERT(suite, "FreeingFirstRegionUpdatesStatistics",
                     num_regions == 1 && num_bytes == 4096,
                     "DMA statistics were incorrect after freeing the first region");
    } else {
        ktest_case_begin(&suite, "FreeingFirstRegionUpdatesStatistics");
        ktest_case_skip(&suite, "FreeingFirstRegionUpdatesStatistics", "region1 allocation failed");
    }

    if (region2 != NULL) {
        dma_free(region2);
        region2 = NULL;
        dma_get_stats(&num_regions, &num_bytes);

        KTEST_ASSERT(suite, "FreeingSecondRegionClearsStatistics",
                     num_regions == 0 && num_bytes == 0,
                     "DMA statistics should have dropped to zero after freeing all regions");
    } else {
        ktest_case_begin(&suite, "FreeingSecondRegionClearsStatistics");
        ktest_case_skip(&suite, "FreeingSecondRegionClearsStatistics", "region2 allocation failed");
    }

    ktest_suite_end(&suite);
}

#endif // ENABLE_KERNEL_TESTS
