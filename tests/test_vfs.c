/*
 * test_vfs.c - VFS and file operations tests
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/fs/vfs.h"
#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/hal/hal_uart.h"
#include "../include/mm/kmalloc.h"

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            hal_uart_puts("  [PASS] " message "\n"); \
            tests_passed++; \
        } else { \
            hal_uart_puts("  [FAIL] " message "\n"); \
            tests_failed++; \
        } \
    } while (0)

/* Global ext2 and VFS contexts */
static ext2_fs_t g_ext2_fs;
static vfs_filesystem_t *g_vfs_fs = NULL;

/**
 * Test 1: Initialize VFS and mount ext2
 */
void test_vfs_init_and_mount(void) {
    hal_uart_puts("\n[TEST] VFS initialization and ext2 mount\n");
    
    /* Initialize VFS */
    int ret = vfs_init();
    TEST_ASSERT(ret == 0, "VFS initialized");
    
    /* Get VirtIO block device */
    virtio_blk_device_t *blk_dev = virtio_blk_get_device();
    TEST_ASSERT(blk_dev != NULL, "VirtIO block device available");
    
    if (!blk_dev) {
        return;
    }
    
    /* Mount ext2 filesystem */
    ret = ext2_mount(&g_ext2_fs, blk_dev);
    TEST_ASSERT(ret == 0, "ext2 filesystem mounted");
    
    if (ret != 0) {
        return;
    }
    
    /* Mount ext2 into VFS */
    g_vfs_fs = ext2_vfs_mount(&g_ext2_fs);
    TEST_ASSERT(g_vfs_fs != NULL, "ext2 mounted into VFS");
    
    if (!g_vfs_fs) {
        return;
    }
    
    /* Mount as root filesystem */
    ret = vfs_mount_root(g_vfs_fs);
    TEST_ASSERT(ret == 0, "VFS root filesystem mounted");
}

/**
 * Test 2: Open and read existing file
 */
void test_vfs_open_and_read(void) {
    hal_uart_puts("\n[TEST] VFS open and read test.txt\n");
    
    /* Open test.txt */
    int fd = vfs_open("/test.txt", O_RDONLY);
    TEST_ASSERT(fd >= 0, "Opened /test.txt for reading");
    
    if (fd < 0) {
        return;
    }
    
    /* Allocate buffer */
    char *buffer = (char *)kmalloc(256);
    TEST_ASSERT(buffer != NULL, "Allocated read buffer");
    
    if (!buffer) {
        vfs_close(fd);
        return;
    }
    
    /* Read file */
    int bytes_read = vfs_read(fd, buffer, 255);
    TEST_ASSERT(bytes_read > 0, "Read data from file");
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        hal_uart_puts("  File contents: \"");
        hal_uart_puts(buffer);
        hal_uart_puts("\"\n");
    }
    
    /* Close file */
    int ret = vfs_close(fd);
    TEST_ASSERT(ret == 0, "Closed file");
    
    kfree(buffer);
}

/**
 * Test 3: File descriptor seek
 */
void test_vfs_seek(void) {
    hal_uart_puts("\n[TEST] VFS seek operations\n");
    
    /* Open file */
    int fd = vfs_open("/test.txt", O_RDONLY);
    TEST_ASSERT(fd >= 0, "Opened file");
    
    if (fd < 0) {
        return;
    }
    
    char buffer[16];
    
    /* Read first 5 bytes */
    int bytes_read = vfs_read(fd, buffer, 5);
    TEST_ASSERT(bytes_read == 5, "Read 5 bytes");
    
    /* Seek back to beginning */
    int pos = vfs_seek(fd, 0, SEEK_SET);
    TEST_ASSERT(pos == 0, "Seek to beginning");
    
    /* Read again */
    bytes_read = vfs_read(fd, buffer, 5);
    TEST_ASSERT(bytes_read == 5, "Read 5 bytes after seek");
    
    /* Seek to end */
    pos = vfs_seek(fd, 0, SEEK_END);
    TEST_ASSERT(pos > 0, "Seek to end");
    
    /* Try to read (should return 0 at EOF) */
    bytes_read = vfs_read(fd, buffer, 10);
    TEST_ASSERT(bytes_read == 0, "Read at EOF returns 0");
    
    vfs_close(fd);
}

/**
 * Test 4: Path resolution
 */
void test_vfs_path_resolution(void) {
    hal_uart_puts("\n[TEST] VFS path resolution\n");
    
    /* Check if test.txt exists */
    int exists = vfs_exists("/test.txt");
    TEST_ASSERT(exists == 1, "/test.txt exists");
    
    /* Check non-existent file */
    exists = vfs_exists("/nonexistent.txt");
    TEST_ASSERT(exists == 0, "/nonexistent.txt does not exist");
    
    /* Get file stats */
    uint32_t size, type;
    int ret = vfs_stat("/test.txt", &size, &type);
    TEST_ASSERT(ret == 0, "vfs_stat succeeded");
    TEST_ASSERT(type == VFS_TYPE_FILE, "test.txt is a file");
    TEST_ASSERT(size > 0, "test.txt has non-zero size");
    
    hal_uart_puts("  File size: ");
    hal_uart_put_uint32(size);
    hal_uart_puts(" bytes\n");
}

/**
 * Test 5: Multiple file descriptors
 */
void test_vfs_multiple_fds(void) {
    hal_uart_puts("\n[TEST] Multiple file descriptors\n");
    
    /* Open same file twice */
    int fd1 = vfs_open("/test.txt", O_RDONLY);
    int fd2 = vfs_open("/test.txt", O_RDONLY);
    
    TEST_ASSERT(fd1 >= 0 && fd2 >= 0, "Opened file twice");
    TEST_ASSERT(fd1 != fd2, "Different file descriptors");
    
    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) vfs_close(fd1);
        if (fd2 >= 0) vfs_close(fd2);
        return;
    }
    
    char buf1[10], buf2[10];
    
    /* Read from fd1 */
    int bytes1 = vfs_read(fd1, buf1, 5);
    
    /* Read from fd2 (independent position) */
    int bytes2 = vfs_read(fd2, buf2, 5);
    
    TEST_ASSERT(bytes1 == 5 && bytes2 == 5, "Both reads succeeded");
    TEST_ASSERT(buf1[0] == buf2[0], "Both read same data");
    
    /* Seek fd1, should not affect fd2 */
    vfs_seek(fd1, 10, SEEK_SET);
    
    /* Read from fd2 should continue from its position */
    bytes2 = vfs_read(fd2, buf2, 5);
    TEST_ASSERT(bytes2 > 0, "fd2 read still works independently");
    
    vfs_close(fd1);
    vfs_close(fd2);
}

/**
 * Run all VFS tests
 */
void test_vfs_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       VFS and File Operations Tests\n");
    hal_uart_puts("========================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    /* Run tests in sequence */
    test_vfs_init_and_mount();
    test_vfs_open_and_read();
    test_vfs_seek();
    test_vfs_path_resolution();
    test_vfs_multiple_fds();
    
    /* Don't unmount - leave filesystem mounted for syscall tests */
    /* ext2_unmount(&g_ext2_fs); */
    
    /* Print summary */
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Tests passed: ");
    hal_uart_put_uint32(tests_passed);
    hal_uart_puts(", Tests failed: ");
    hal_uart_put_uint32(tests_failed);
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("*** ALL TESTS PASSED ***\n");
    } else {
        hal_uart_puts("*** SOME TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n");
}
