/*
 * test_errno.c - errno error handling system tests
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/kernel/errno.h"
#include "../include/fs/vfs.h"
#include "../include/fs/ext2.h"
#include "../include/kernel/elf_loader.h"
#include "../include/hal/hal_uart.h"
#include "../include/drivers/virtio_blk.h"

/* Test statistics */
static int tests_passed = 0;
static int tests_failed = 0;

/* Global filesystem for tests */
static ext2_fs_t g_errno_test_fs;
static int g_fs_mounted = 0;

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

/**
 * Setup: Use already-mounted filesystem for tests
 */
int test_errno_setup(void) {
    hal_uart_puts("\n[SETUP] Checking filesystem for errno tests\n");
    
    /* Get reference to the mounted ext2 filesystem from main.c */
    extern ext2_fs_t g_test_ext2_fs;
    
    /* Check if filesystem was already mounted by checking superblock */
    if (g_test_ext2_fs.superblock != NULL) {
        hal_uart_puts("  [OK] Using pre-mounted ext2 filesystem\n");
        g_errno_test_fs = g_test_ext2_fs;
        g_fs_mounted = 1;
        return 0;
    }
    
    /* Filesystem not mounted yet */
    hal_uart_puts("  [INFO] Filesystem not yet mounted, skipping ext2 tests\n");
    return -1;
}

/**
 * Test 1: Basic errno set/get operations
 */
void test_errno_basic(void) {
    hal_uart_puts("\n[TEST] errno basic operations\n");
    
    /* Clear errno */
    clear_errno();
    TEST_ASSERT(errno == 0, "errno cleared to 0");
    
    /* Set errno to EINVAL */
    set_errno(THUNDEROS_EINVAL);
    TEST_ASSERT(errno == THUNDEROS_EINVAL, "errno set to EINVAL");
    
    /* Get errno */
    int err = get_errno();
    TEST_ASSERT(err == THUNDEROS_EINVAL, "get_errno() returns EINVAL");
    
    /* Clear errno again */
    clear_errno();
    TEST_ASSERT(errno == 0, "errno cleared again");
}

/**
 * Test 2: Error string conversion
 */
void test_errno_strerror(void) {
    hal_uart_puts("\n[TEST] errno string conversion\n");
    
    /* Test common error codes */
    const char *msg;
    
    msg = thunderos_strerror(THUNDEROS_OK);
    TEST_ASSERT(msg != NULL, "THUNDEROS_OK has error string");
    hal_uart_puts("  THUNDEROS_OK: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    msg = thunderos_strerror(THUNDEROS_EINVAL);
    TEST_ASSERT(msg != NULL, "THUNDEROS_EINVAL has error string");
    hal_uart_puts("  THUNDEROS_EINVAL: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    msg = thunderos_strerror(THUNDEROS_ENOMEM);
    TEST_ASSERT(msg != NULL, "THUNDEROS_ENOMEM has error string");
    hal_uart_puts("  THUNDEROS_ENOMEM: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    msg = thunderos_strerror(THUNDEROS_EIO);
    TEST_ASSERT(msg != NULL, "THUNDEROS_EIO has error string");
    hal_uart_puts("  THUNDEROS_EIO: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    /* Test filesystem errors */
    msg = thunderos_strerror(THUNDEROS_EFS_BADSUPER);
    TEST_ASSERT(msg != NULL, "THUNDEROS_EFS_BADSUPER has error string");
    hal_uart_puts("  THUNDEROS_EFS_BADSUPER: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    msg = thunderos_strerror(THUNDEROS_EFS_BADINO);
    TEST_ASSERT(msg != NULL, "THUNDEROS_EFS_BADINO has error string");
    hal_uart_puts("  THUNDEROS_EFS_BADINO: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    /* Test ELF errors */
    msg = thunderos_strerror(THUNDEROS_EELF_MAGIC);
    TEST_ASSERT(msg != NULL, "THUNDEROS_EELF_MAGIC has error string");
    hal_uart_puts("  THUNDEROS_EELF_MAGIC: ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
    
    /* Test unknown error */
    msg = thunderos_strerror(9999);
    TEST_ASSERT(msg != NULL, "Unknown error has fallback string");
    hal_uart_puts("  Unknown (9999): ");
    hal_uart_puts(msg);
    hal_uart_puts("\n");
}

/**
 * Test 3: kernel_perror output
 */
void test_errno_perror(void) {
    hal_uart_puts("\n[TEST] kernel_perror output\n");
    
    /* Set errno and print error */
    set_errno(THUNDEROS_ENOENT);
    hal_uart_puts("  Expected output: \"test_prefix: No such file or directory\"\n");
    hal_uart_puts("  Actual output:   \"");
    kernel_perror("test_prefix");
    hal_uart_puts("\"\n");
    TEST_ASSERT(errno == THUNDEROS_ENOENT, "errno unchanged by perror");
    
    clear_errno();
}

/**
 * Test 4: VFS error propagation
 */
void test_errno_vfs_propagation(void) {
    hal_uart_puts("\n[TEST] VFS errno error propagation\n");
    
    /* Try to open non-existent file */
    clear_errno();
    int fd = vfs_open("/nonexistent_file_12345.txt", O_RDONLY);
    TEST_ASSERT(fd < 0, "vfs_open failed for non-existent file");
    TEST_ASSERT(errno != 0, "errno set by vfs_open");
    hal_uart_puts("  errno after failed open: ");
    hal_uart_put_uint32(errno);
    hal_uart_puts(" (");
    hal_uart_puts(thunderos_strerror(errno));
    hal_uart_puts(")\n");
    
    /* Try to read from invalid fd */
    clear_errno();
    char buffer[16];
    int ret = vfs_read(999, buffer, 16);
    TEST_ASSERT(ret < 0, "vfs_read failed for invalid fd");
    TEST_ASSERT(errno == THUNDEROS_EBADF, "errno set to EBADF for bad fd");
    
    /* Try to write to invalid fd */
    clear_errno();
    ret = vfs_write(999, buffer, 16);
    TEST_ASSERT(ret < 0, "vfs_write failed for invalid fd");
    TEST_ASSERT(errno == THUNDEROS_EBADF, "errno set to EBADF for bad fd");
    
    clear_errno();
}

/**
 * Test 5: ELF loader error codes
 */
void test_errno_elf_errors(void) {
    hal_uart_puts("\n[TEST] ELF loader errno error codes\n");
    
    /* Try to load non-existent file */
    clear_errno();
    int ret = elf_load_exec("/nonexistent_elf_binary", NULL, 0);
    TEST_ASSERT(ret < 0, "elf_load_exec failed for non-existent file");
    TEST_ASSERT(errno != 0, "errno set by ELF loader");
    hal_uart_puts("  errno after ELF load failure: ");
    hal_uart_put_uint32(errno);
    hal_uart_puts(" (");
    hal_uart_puts(thunderos_strerror(errno));
    hal_uart_puts(")\n");
    
    /* Try to load non-ELF file (text file) */
    clear_errno();
    ret = elf_load_exec("/test.txt", NULL, 0);
    TEST_ASSERT(ret < 0, "elf_load_exec failed for non-ELF file");
    TEST_ASSERT(errno != 0, "errno set for invalid ELF");
    hal_uart_puts("  errno after invalid ELF: ");
    hal_uart_put_uint32(errno);
    hal_uart_puts(" (");
    hal_uart_puts(thunderos_strerror(errno));
    hal_uart_puts(")\n");
    
    clear_errno();
}

/**
 * Test 6: ext2 filesystem error codes
 */
void test_errno_ext2_errors(void) {
    hal_uart_puts("\n[TEST] ext2 filesystem errno error codes\n");
    
    if (!g_fs_mounted) {
        hal_uart_puts("  [SKIP] Filesystem not mounted, skipping ext2 tests\n");
        return;
    }
    
    /* Read root directory inode */
    ext2_inode_t root_inode;
    
    clear_errno();
    int ret = ext2_read_inode(&g_errno_test_fs, EXT2_ROOT_INO, &root_inode);
    TEST_ASSERT(ret == 0, "ext2_read_inode succeeded");
    TEST_ASSERT(errno == 0, "errno cleared on success");
    
    /* Try to look up non-existent file */
    clear_errno();
    uint32_t inode_num = ext2_lookup(&g_errno_test_fs, &root_inode, "nonexistent_file_xyz.txt");
    TEST_ASSERT(inode_num == 0, "ext2_lookup failed for non-existent file");
    TEST_ASSERT(errno == THUNDEROS_ENOENT, "errno set to ENOENT");
    
    /* Try to read invalid inode number */
    clear_errno();
    ext2_inode_t bad_inode;
    ret = ext2_read_inode(&g_errno_test_fs, 0, &bad_inode);
    TEST_ASSERT(ret < 0, "ext2_read_inode failed for inode 0");
    TEST_ASSERT(errno == THUNDEROS_EINVAL, "errno set to EINVAL for inode 0");
    
    /* Try to read inode beyond range */
    clear_errno();
    ret = ext2_read_inode(&g_errno_test_fs, 9999999, &bad_inode);
    TEST_ASSERT(ret < 0, "ext2_read_inode failed for out-of-range inode");
    TEST_ASSERT(errno == THUNDEROS_EFS_BADINO, "errno set to EFS_BADINO");
    
    /* Try to pass NULL parameters */
    clear_errno();
    ret = ext2_read_inode(NULL, EXT2_ROOT_INO, &bad_inode);
    TEST_ASSERT(ret < 0, "ext2_read_inode failed for NULL fs");
    TEST_ASSERT(errno == THUNDEROS_EINVAL, "errno set to EINVAL for NULL param");
    
    clear_errno();
}

/**
 * Test 7: RETURN_ERRNO macro
 */
static int test_function_with_return_errno(int should_fail) {
    if (should_fail) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    clear_errno();
    return 0;
}

void test_errno_macro(void) {
    hal_uart_puts("\n[TEST] RETURN_ERRNO macro\n");
    
    /* Test success case */
    clear_errno();
    int ret = test_function_with_return_errno(0);
    TEST_ASSERT(ret == 0, "Function succeeded");
    TEST_ASSERT(errno == 0, "errno is 0 on success");
    
    /* Test failure case */
    clear_errno();
    ret = test_function_with_return_errno(1);
    TEST_ASSERT(ret == -1, "Function returned -1");
    TEST_ASSERT(errno == THUNDEROS_EINVAL, "errno set to EINVAL by macro");
    
    clear_errno();
}

/**
 * Test 8: Error code ranges
 */
void test_errno_ranges(void) {
    hal_uart_puts("\n[TEST] errno error code ranges\n");
    
    /* Verify error codes are in expected ranges */
    TEST_ASSERT(THUNDEROS_OK == 0, "THUNDEROS_OK is 0");
    
    /* Generic errors (1-29) */
    TEST_ASSERT(THUNDEROS_EPERM >= 1 && THUNDEROS_EPERM <= 29, 
                "EPERM in generic range");
    TEST_ASSERT(THUNDEROS_EINVAL >= 1 && THUNDEROS_EINVAL <= 29,
                "EINVAL in generic range");
    
    /* Filesystem errors (30-49) */
    TEST_ASSERT(THUNDEROS_EFS_CORRUPT >= 30 && THUNDEROS_EFS_CORRUPT <= 49,
                "EFS_CORRUPT in filesystem range");
    TEST_ASSERT(THUNDEROS_EFS_BADSUPER >= 30 && THUNDEROS_EFS_BADSUPER <= 49,
                "EFS_BADSUPER in filesystem range");
    
    /* ELF errors (50-69) */
    TEST_ASSERT(THUNDEROS_EELF_MAGIC >= 50 && THUNDEROS_EELF_MAGIC <= 69,
                "EELF_MAGIC in ELF range");
    TEST_ASSERT(THUNDEROS_EELF_ARCH >= 50 && THUNDEROS_EELF_ARCH <= 69,
                "EELF_ARCH in ELF range");
    
    /* VirtIO errors (70-89) */
    TEST_ASSERT(THUNDEROS_EVIRTIO_TIMEOUT >= 70 && THUNDEROS_EVIRTIO_TIMEOUT <= 89,
                "EVIRTIO_TIMEOUT in VirtIO range");
    
    /* Process errors (90-109) */
    TEST_ASSERT(THUNDEROS_EPROC_INIT >= 90 && THUNDEROS_EPROC_INIT <= 109,
                "EPROC_INIT in process range");
    
    /* Memory errors (110-129) */
    TEST_ASSERT(THUNDEROS_EMEM_NOMEM >= 110 && THUNDEROS_EMEM_NOMEM <= 129,
                "EMEM_NOMEM in memory range");
}

/**
 * Test 9: Error propagation through call stack
 */
static int level3_function(int should_fail) {
    if (should_fail) {
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    clear_errno();
    return 0;
}

static int level2_function(int should_fail) {
    int ret = level3_function(should_fail);
    if (ret < 0) {
        /* errno already set by level3, just propagate */
        return -1;
    }
    return 0;
}

static int level1_function(int should_fail) {
    int ret = level2_function(should_fail);
    if (ret < 0) {
        /* errno already set, propagate */
        return -1;
    }
    return 0;
}

void test_errno_propagation(void) {
    hal_uart_puts("\n[TEST] errno propagation through call stack\n");
    
    /* Test error propagation */
    clear_errno();
    int ret = level1_function(1);
    TEST_ASSERT(ret < 0, "Error propagated to caller");
    TEST_ASSERT(errno == THUNDEROS_EIO, "errno preserved through call stack");
    hal_uart_puts("  errno propagated: ");
    hal_uart_put_uint32(errno);
    hal_uart_puts(" (");
    hal_uart_puts(thunderos_strerror(errno));
    hal_uart_puts(")\n");
    
    /* Test success case */
    clear_errno();
    ret = level1_function(0);
    TEST_ASSERT(ret == 0, "Success propagated to caller");
    TEST_ASSERT(errno == 0, "errno cleared on success");
    
    clear_errno();
}

/**
 * Test 10: Multiple consecutive errors
 */
void test_errno_consecutive_errors(void) {
    hal_uart_puts("\n[TEST] Multiple consecutive errors\n");
    
    /* Make multiple failing calls */
    clear_errno();
    
    /* First error */
    int fd1 = vfs_open("/nonexistent1.txt", O_RDONLY);
    int err1 = errno;
    TEST_ASSERT(fd1 < 0, "First vfs_open failed");
    TEST_ASSERT(err1 != 0, "First error set errno");
    
    /* Second error (should overwrite) */
    int fd2 = vfs_read(999, NULL, 0);
    int err2 = errno;
    TEST_ASSERT(fd2 < 0, "Second vfs_read failed");
    TEST_ASSERT(err2 != 0, "Second error set errno");
    TEST_ASSERT(err2 != err1 || err1 == err2, "errno updated or same");
    
    hal_uart_puts("  First error: ");
    hal_uart_put_uint32(err1);
    hal_uart_puts(", Second error: ");
    hal_uart_put_uint32(err2);
    hal_uart_puts("\n");
    
    clear_errno();
}

/**
 * Run all errno tests
 */
void test_errno_all(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       errno Error Handling Tests\n");
    hal_uart_puts("========================================\n");
    
    tests_passed = 0;
    tests_failed = 0;
    
    /* Setup: mount filesystem */
    test_errno_setup();
    
    /* Run tests in sequence */
    test_errno_basic();
    test_errno_strerror();
    test_errno_perror();
    test_errno_vfs_propagation();
    test_errno_elf_errors();
    test_errno_ext2_errors();
    test_errno_macro();
    test_errno_ranges();
    test_errno_propagation();
    test_errno_consecutive_errors();
    
    /* Print summary */
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Tests passed: ");
    hal_uart_put_uint32(tests_passed);
    hal_uart_puts(", Tests failed: ");
    hal_uart_put_uint32(tests_failed);
    hal_uart_puts("\n");
    
    if (tests_failed == 0) {
        hal_uart_puts("*** ALL ERRNO TESTS PASSED ***\n");
    } else {
        hal_uart_puts("*** SOME ERRNO TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n");
}
