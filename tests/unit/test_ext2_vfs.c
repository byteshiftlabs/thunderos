/*
 * VFS + ext2 Filesystem Unit Tests
 *
 * Tests the VFS layer and ext2 filesystem against the mounted root
 * filesystem. These tests run after the filesystem is initialized.
 *
 * Compile with: ENABLE_KERNEL_TESTS=1
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "fs/vfs.h"
#include "kernel/kstring.h"
#include <stdint.h>
#include <stddef.h>

/* Temporary file used for create/write/delete tests */
#define TEST_FILE_PATH    "/vfs_unit_test.txt"
#define TEST_WRITE_DATA   "ThunderOS VFS test\n"
#define TEST_WRITE_SIZE   19

/* ----------------------------------------------------------------------- */

static int test_vfs_stat_root(int *passed, int *failed)
{
    hal_uart_puts("[TEST] stat root '/'\n");

    uint32_t size = 0, type = 0;
    int ret = vfs_stat("/", &size, &type);
    if (ret != 0) {
        hal_uart_puts("  [FAIL] vfs_stat('/') returned -1\n");
        (*failed)++;
        return -1;
    }
    if (type != VFS_TYPE_DIRECTORY) {
        hal_uart_puts("  [FAIL] root is not VFS_TYPE_DIRECTORY\n");
        (*failed)++;
        return -1;
    }
    hal_uart_puts("  [PASS] root is a directory\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_stat_file(int *passed, int *failed)
{
    hal_uart_puts("[TEST] stat '/hello.txt'\n");

    uint32_t size = 0, type = 0;
    int ret = vfs_stat("/hello.txt", &size, &type);
    if (ret != 0) {
        hal_uart_puts("  [FAIL] vfs_stat returned -1\n");
        (*failed)++;
        return -1;
    }
    if (type != VFS_TYPE_FILE) {
        hal_uart_puts("  [FAIL] /hello.txt is not VFS_TYPE_FILE\n");
        (*failed)++;
        return -1;
    }
    if (size == 0) {
        hal_uart_puts("  [FAIL] /hello.txt has zero size\n");
        (*failed)++;
        return -1;
    }
    hal_uart_puts("  [PASS] size=");
    kprint_dec(size);
    hal_uart_puts(" type=FILE\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_read_file(int *passed, int *failed)
{
    hal_uart_puts("[TEST] open + read '/hello.txt'\n");

    int fd = vfs_open("/hello.txt", O_RDONLY, 0);
    if (fd < 0) {
        hal_uart_puts("  [FAIL] vfs_open returned -1\n");
        (*failed)++;
        return -1;
    }

    uint8_t buf[64] = {0};
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    vfs_close(fd);

    if (n <= 0) {
        hal_uart_puts("  [FAIL] vfs_read returned <= 0\n");
        (*failed)++;
        return -1;
    }
    hal_uart_puts("  [PASS] read ");
    kprint_dec((size_t)n);
    hal_uart_puts(" bytes\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_create_file(int *passed, int *failed)
{
    hal_uart_puts("[TEST] create '" TEST_FILE_PATH "'\n");

    /* Remove any leftover from a prior run */
    vfs_unlink(TEST_FILE_PATH);

    int fd = vfs_open(TEST_FILE_PATH, O_WRONLY | O_CREAT, VFS_DEFAULT_FILE_MODE);
    if (fd < 0) {
        hal_uart_puts("  [FAIL] vfs_open(O_CREAT) returned -1\n");
        (*failed)++;
        return -1;
    }
    vfs_close(fd);

    uint32_t size = 0, type = 0;
    int ret = vfs_stat(TEST_FILE_PATH, &size, &type);
    if (ret != 0 || type != VFS_TYPE_FILE) {
        hal_uart_puts("  [FAIL] stat after create failed\n");
        (*failed)++;
        return -1;
    }
    hal_uart_puts("  [PASS] file created and stat'd\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_write_file(int *passed, int *failed)
{
    hal_uart_puts("[TEST] write + read back '" TEST_FILE_PATH "'\n");

    /* Write */
    int fd = vfs_open(TEST_FILE_PATH, O_WRONLY, 0);
    if (fd < 0) {
        hal_uart_puts("  [FAIL] vfs_open(O_WRONLY) returned -1\n");
        (*failed)++;
        return -1;
    }
    int n = vfs_write(fd, TEST_WRITE_DATA, TEST_WRITE_SIZE);
    vfs_close(fd);
    if (n != TEST_WRITE_SIZE) {
        hal_uart_puts("  [FAIL] vfs_write returned wrong count\n");
        (*failed)++;
        return -1;
    }

    /* Read back and verify */
    fd = vfs_open(TEST_FILE_PATH, O_RDONLY, 0);
    if (fd < 0) {
        hal_uart_puts("  [FAIL] vfs_open(O_RDONLY) for readback returned -1\n");
        (*failed)++;
        return -1;
    }
    uint8_t buf[32] = {0};
    int rn = vfs_read(fd, buf, sizeof(buf));
    vfs_close(fd);
    if (rn != TEST_WRITE_SIZE) {
        hal_uart_puts("  [FAIL] vfs_read returned wrong count\n");
        (*failed)++;
        return -1;
    }
    for (int i = 0; i < TEST_WRITE_SIZE; i++) {
        if (buf[i] != (uint8_t)TEST_WRITE_DATA[i]) {
            hal_uart_puts("  [FAIL] data mismatch at byte ");
            kprint_dec((size_t)i);
            hal_uart_puts("\n");
            (*failed)++;
            return -1;
        }
    }
    hal_uart_puts("  [PASS] ");
    kprint_dec(TEST_WRITE_SIZE);
    hal_uart_puts(" bytes written and verified\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_delete_file(int *passed, int *failed)
{
    hal_uart_puts("[TEST] delete '" TEST_FILE_PATH "'\n");

    int ret = vfs_unlink(TEST_FILE_PATH);
    if (ret != 0) {
        hal_uart_puts("  [FAIL] vfs_unlink returned -1\n");
        (*failed)++;
        return -1;
    }

    uint32_t size = 0, type = 0;
    ret = vfs_stat(TEST_FILE_PATH, &size, &type);
    if (ret == 0) {
        hal_uart_puts("  [FAIL] file still exists after unlink\n");
        (*failed)++;
        return -1;
    }
    hal_uart_puts("  [PASS] file deleted and confirmed gone\n");
    (*passed)++;
    return 0;
}

/* ----------------------------------------------------------------------- */

void test_vfs_all(void)
{
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("       VFS + ext2 Filesystem Tests\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("\n");

    int passed = 0;
    int failed = 0;

    test_vfs_stat_root(&passed, &failed);
    test_vfs_stat_file(&passed, &failed);
    test_vfs_read_file(&passed, &failed);
    test_vfs_create_file(&passed, &failed);
    test_vfs_write_file(&passed, &failed);
    test_vfs_delete_file(&passed, &failed);

    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("Tests passed: ");
    kprint_dec((size_t)passed);
    hal_uart_puts(", Tests failed: ");
    kprint_dec((size_t)failed);
    hal_uart_puts("\n");
    if (failed == 0) {
        hal_uart_puts("*** ALL TESTS PASSED ***\n");
    } else {
        hal_uart_puts("*** SOME TESTS FAILED ***\n");
    }
    hal_uart_puts("========================================\n");
}

#endif /* ENABLE_KERNEL_TESTS */
