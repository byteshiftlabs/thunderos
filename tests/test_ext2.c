/*
 * test_ext2.c - ext2 filesystem tests
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/fs/ext2.h"
#include "../include/drivers/virtio_blk.h"
#include "../include/hal/hal_uart.h"
#include "../include/mm/kmalloc.h"
#include "../include/mm/dma.h"

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test result macros */
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

/* Global ext2 filesystem context */
static ext2_fs_t g_ext2_fs;

/**
 * Test 1: Mount ext2 filesystem and verify superblock
 */
void test_ext2_mount(void) {
    hal_uart_puts("\n[TEST] ext2 mount and superblock validation\n");
    
    /* Get VirtIO block device */
    virtio_blk_device_t *blk_dev = virtio_blk_get_device();
    TEST_ASSERT(blk_dev != NULL, "VirtIO block device available");
    
    if (!blk_dev) {
        return;
    }
    
    /* Mount ext2 filesystem */
    int ret = ext2_mount(&g_ext2_fs, blk_dev);
    TEST_ASSERT(ret == 0, "ext2_mount() succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Verify superblock magic */
    TEST_ASSERT(g_ext2_fs.superblock != NULL, "Superblock loaded");
    TEST_ASSERT(g_ext2_fs.superblock->s_magic == EXT2_SUPER_MAGIC, 
                "Superblock magic is 0xEF53");
    
    /* Verify block size is reasonable */
    TEST_ASSERT(g_ext2_fs.block_size >= EXT2_MIN_BLOCK_SIZE && 
                g_ext2_fs.block_size <= EXT2_MAX_BLOCK_SIZE,
                "Block size is valid");
    
    /* Verify we have at least one block group */
    TEST_ASSERT(g_ext2_fs.num_groups > 0, "At least one block group exists");
    
    /* Print filesystem info */
    hal_uart_puts("  Filesystem info:\n");
    hal_uart_puts("    Total inodes: ");
    hal_uart_put_uint32(g_ext2_fs.superblock->s_inodes_count);
    hal_uart_puts("\n");
    
    hal_uart_puts("    Total blocks: ");
    hal_uart_put_uint32(g_ext2_fs.superblock->s_blocks_count);
    hal_uart_puts("\n");
    
    hal_uart_puts("    Block size: ");
    hal_uart_put_uint32(g_ext2_fs.block_size);
    hal_uart_puts(" bytes\n");
    
    hal_uart_puts("    Block groups: ");
    hal_uart_put_uint32(g_ext2_fs.num_groups);
    hal_uart_puts("\n");
}

/**
 * Test 2: Read root directory inode
 */
void test_ext2_read_root_inode(void) {
    hal_uart_puts("\n[TEST] ext2 read root directory inode\n");
    
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret == 0) {
        /* Verify it's a directory */
        TEST_ASSERT((root_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR,
                    "Root inode is a directory");
        
        /* Verify it has reasonable size */
        TEST_ASSERT(root_inode.i_size > 0, "Root directory has non-zero size");
        
        hal_uart_puts("  Root directory size: ");
        hal_uart_put_uint32(root_inode.i_size);
        hal_uart_puts(" bytes\n");
    }
}

/**
 * Test 3: List root directory contents
 */
static void dir_entry_callback(const char *name, uint32_t inode, uint8_t type) {
    hal_uart_puts("    ");
    
    /* Print file type indicator */
    if (type == EXT2_FT_DIR) {
        hal_uart_puts("[DIR]  ");
    } else if (type == EXT2_FT_REG_FILE) {
        hal_uart_puts("[FILE] ");
    } else {
        hal_uart_puts("[????] ");
    }
    
    /* Print inode number */
    hal_uart_put_uint32(inode);
    hal_uart_puts(" ");
    
    /* Print name */
    hal_uart_puts(name);
    hal_uart_puts("\n");
}

void test_ext2_list_root_dir(void) {
    hal_uart_puts("\n[TEST] ext2 list root directory\n");
    
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret == 0) {
        hal_uart_puts("  Root directory contents:\n");
        ret = ext2_list_dir(&g_ext2_fs, &root_inode, dir_entry_callback);
        TEST_ASSERT(ret == 0, "List directory succeeded");
    }
}

/**
 * Test 4: Read a test file from filesystem
 */
void test_ext2_read_file(void) {
    hal_uart_puts("\n[TEST] ext2 read test file\n");
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    int ret = ext2_read_inode(&g_ext2_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "Read root inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Look up "test.txt" in root directory */
    uint32_t test_inode_num = ext2_lookup(&g_ext2_fs, &root_inode, "test.txt");
    TEST_ASSERT(test_inode_num != 0, "Found test.txt in root directory");
    
    if (test_inode_num == 0) {
        return;
    }
    
    /* Read test file inode */
    ext2_inode_t test_inode;
    ret = ext2_read_inode(&g_ext2_fs, test_inode_num, &test_inode);
    TEST_ASSERT(ret == 0, "Read test.txt inode succeeded");
    
    if (ret != 0) {
        return;
    }
    
    /* Verify it's a regular file */
    TEST_ASSERT((test_inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG,
                "test.txt is a regular file");
    
    /* Allocate buffer for file contents */
    uint32_t file_size = test_inode.i_size;
    char *buffer = (char *)kmalloc(file_size + 1);
    TEST_ASSERT(buffer != NULL, "Allocated buffer for file");
    
    if (!buffer) {
        return;
    }
    
    /* Read file contents */
    ret = ext2_read_file(&g_ext2_fs, &test_inode, 0, buffer, file_size);
    TEST_ASSERT(ret == (int)file_size, "Read complete file contents");
    
    if (ret > 0) {
        buffer[ret] = '\0';  /* Null terminate */
        hal_uart_puts("  File contents: \"");
        hal_uart_puts(buffer);
        hal_uart_puts("\"\n");
    }
    
    kfree(buffer);
}

/**
 * Run all ext2 tests
 */
void test_ext2_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       ext2 Filesystem Tests\n");
    hal_uart_puts("========================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    /* Run tests in sequence */
    test_ext2_mount();
    test_ext2_read_root_inode();
    test_ext2_list_root_dir();
    test_ext2_read_file();
    
    /* Unmount filesystem */
    ext2_unmount(&g_ext2_fs);
    
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
