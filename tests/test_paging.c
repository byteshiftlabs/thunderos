/*
 * Virtual Memory (Paging) Tests
 */

#include "framework/kunit.h"
#include "mm/paging.h"
#include "mm/pmm.h"

static void test_virt_to_phys(void) {
    // Test identity mapping
    page_table_t *pt = get_kernel_page_table();
    
    uintptr_t vaddr = 0x80200000;  // Kernel start
    uintptr_t paddr;
    
    int result = virt_to_phys(pt, vaddr, &paddr);
    
    ASSERT_EQ(result, 0);
    ASSERT_EQ(paddr, vaddr);  // Identity mapped
}

static void test_map_unmap(void) {
    page_table_t *pt = get_kernel_page_table();
    
    // Allocate a physical page
    uintptr_t phys_page = pmm_alloc_page();
    ASSERT_NE(phys_page, 0);
    
    // Map it to a virtual address
    uintptr_t virt_addr = 0x40000000;  // Arbitrary user space address
    int result = map_page(pt, virt_addr, phys_page, PTE_USER_DATA);
    ASSERT_EQ(result, 0);
    
    // Verify translation works
    uintptr_t translated;
    result = virt_to_phys(pt, virt_addr, &translated);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(translated, phys_page);
    
    // Unmap it
    result = unmap_page(pt, virt_addr);
    ASSERT_EQ(result, 0);
    
    // Verify translation fails now
    result = virt_to_phys(pt, virt_addr, &translated);
    ASSERT_NE(result, 0);
    
    // Free physical page
    pmm_free_page(phys_page);
}

static void test_kernel_helpers(void) {
    // Test kernel address conversion
    uintptr_t phys = 0x80200000;
    uintptr_t virt = kernel_phys_to_virt(phys);
    
    // For identity mapping, virt == phys
    ASSERT_EQ(virt, phys);
    
    uintptr_t phys2 = kernel_virt_to_phys(virt);
    ASSERT_EQ(phys2, phys);
}

int main(void) {
    KUNIT_RUN_TEST(test_virt_to_phys);
    KUNIT_RUN_TEST(test_map_unmap);
    KUNIT_RUN_TEST(test_kernel_helpers);
    
    return KUNIT_REPORT();
}
