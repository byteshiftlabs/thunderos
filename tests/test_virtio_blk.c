/*
 * VirtIO Block Device Test Suite
 * 
 * Tests VirtIO block driver initialization and I/O operations
 */

#include <stdint.h>
#include <stdbool.h>
#include <drivers/virtio_blk.h>
#include <mm/dma.h>
#include <hal/hal_uart.h>

// Test results tracking
static int tests_passed = 0;
static int tests_failed = 0;

// Simple assertion macro
#define ASSERT(condition, message) \
    do { \
        if (condition) { \
            hal_uart_puts("[PASS] "); \
            hal_uart_puts(message); \
            hal_uart_puts("\n"); \
            tests_passed++; \
        } else { \
            hal_uart_puts("[FAIL] "); \
            hal_uart_puts(message); \
            hal_uart_puts("\n"); \
            tests_failed++; \
        } \
    } while(0)

/**
 * Test 1: Initialize VirtIO block device
 */
static void test_virtio_blk_init(void) {
    hal_uart_puts("\n=== Test 1: VirtIO Block Device Initialization ===\n");
    
    // Initialize DMA allocator first
    dma_init();
    
    // Scan VirtIO MMIO addresses to find block device
    // QEMU virt machine has VirtIO devices at 0x10001000 - 0x10008000
    uint64_t virtio_addrs[] = {
        0x10001000, 0x10002000, 0x10003000, 0x10004000,
        0x10005000, 0x10006000, 0x10007000, 0x10008000
    };
    
    int result = -1;
    for (int i = 0; i < 8; i++) {
        result = virtio_blk_init(virtio_addrs[i], 1 + i);
        if (result == 0) {
            break;  // Found and initialized block device
        }
    }
    
    ASSERT(result == 0, "Device initialized successfully");
}

/**
 * Test 2: Check device capacity
 */
static void test_virtio_blk_capacity(void) {
    hal_uart_puts("\n=== Test 2: Check Device Capacity ===\n");
    
    uint64_t capacity = virtio_blk_get_capacity();
    
    ASSERT(capacity > 0, "Device has non-zero capacity");
    ASSERT(capacity < 0xFFFFFFFFFFFFFFFFULL, "Capacity is reasonable");
}

/**
 * Test 3: Read sector 0
 */
static void test_virtio_blk_read_sector0(void) {
    hal_uart_puts("\n=== Test 3: Read Sector 0 ===\n");
    
    // Allocate DMA buffer for one sector (512 bytes)
    dma_region_t *dma_buf = dma_alloc(512, 0);
    ASSERT(dma_buf != NULL, "Allocated DMA buffer for read");
    
    if (dma_buf == NULL) {
        return;
    }
    
    uint8_t *buffer = (uint8_t*)dma_buf->virt_addr;
    
    // Clear buffer
    for (int i = 0; i < 512; i++) {
        buffer[i] = 0;
    }
    
    // Read sector 0 (count is in sectors)
    int result = virtio_blk_read(0, buffer, 1);
    
    ASSERT(result >= 0, "Read operation completed successfully");
    
    hal_uart_puts("Read completed (data may be zeros for empty disk)\n");
    
    dma_free(dma_buf);
}

/**
 * Test 4: Write and read back data
 */
static void test_virtio_blk_write_read(void) {
    hal_uart_puts("\n=== Test 4: Write and Read Back ===\n");
    
    // Allocate DMA buffers
    dma_region_t *write_dma = dma_alloc(512, 0);
    dma_region_t *read_dma = dma_alloc(512, 0);
    
    ASSERT(write_dma != NULL, "Allocated write buffer");
    ASSERT(read_dma != NULL, "Allocated read buffer");
    
    if (write_dma == NULL || read_dma == NULL) {
        if (write_dma) dma_free(write_dma);
        if (read_dma) dma_free(read_dma);
        return;
    }
    
    uint8_t *write_buffer = (uint8_t*)write_dma->virt_addr;
    uint8_t *read_buffer = (uint8_t*)read_dma->virt_addr;
    
    // Fill write buffer with test pattern
    for (int i = 0; i < 512; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    // Write to sector 1 (avoid sector 0, count is in sectors)
    int write_result = virtio_blk_write(1, write_buffer, 1);
    ASSERT(write_result >= 0, "Write operation completed");
    
    // Clear read buffer
    for (int i = 0; i < 512; i++) {
        read_buffer[i] = 0;
    }
    
    // Read back the data (count is in sectors)
    int read_result = virtio_blk_read(1, read_buffer, 1);
    ASSERT(read_result >= 0, "Read operation completed");
    
    // Verify data matches
    bool data_matches = true;
    for (int i = 0; i < 512; i++) {
        if (write_buffer[i] != read_buffer[i]) {
            data_matches = false;
            break;
        }
    }
    
    ASSERT(data_matches, "Read data matches written data");
    
    dma_free(write_dma);
    dma_free(read_dma);
}

/**
 * Run all VirtIO block device tests
 */
void test_virtio_blk_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  VirtIO Block Device Test Suite\n");
    hal_uart_puts("========================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    // Run all tests
    test_virtio_blk_init();
    test_virtio_blk_capacity();
    test_virtio_blk_read_sector0();
    test_virtio_blk_write_read();
    
    // Print summary
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("  Test Summary\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("Tests passed: ");
    // Simple number printing (assuming < 100 tests)
    if (tests_passed >= 10) {
        hal_uart_puts("1");
        hal_uart_puts((const char[]){'0' + (tests_passed % 10), '\0'});
    } else {
        hal_uart_puts((const char[]){'0' + tests_passed, '\0'});
    }
    hal_uart_puts("\n");
    
    hal_uart_puts("Tests failed: ");
    if (tests_failed >= 10) {
        hal_uart_puts("1");
        hal_uart_puts((const char[]){'0' + (tests_failed % 10), '\0'});
    } else {
        hal_uart_puts((const char[]){'0' + tests_failed, '\0'});
    }
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("\n*** ALL TESTS PASSED ***\n");
    } else {
        hal_uart_puts("\n*** SOME TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n\n");
}
