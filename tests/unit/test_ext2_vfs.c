/*
 * VFS + ext2 Filesystem Unit Tests
 *
 * Tests the VFS layer and ext2 filesystem against the mounted root
 * filesystem. These tests run after the filesystem is initialized.
 *
 * Compile with: ENABLE_KERNEL_TESTS=1
 */

#ifdef ENABLE_KERNEL_TESTS

#include "fs/ext2.h"
#include "fs/vfs.h"
#include "kernel/errno.h"
#include "kernel/kstring.h"
#include "tests/structured_test_kernel.h"
#include <stdint.h>
#include <stddef.h>

/* Temporary file used for create/write/delete tests */
#define TEST_FILE_PATH    "/vfs_unit_test.txt"
#define TEST_WRITE_DATA   "ThunderOS VFS test\n"
#define TEST_WRITE_SIZE   19

/* ----------------------------------------------------------------------- */

static ktest_suite_t *g_suite = NULL;

static int test_vfs_stat_root(void)
{
    ktest_case_begin(g_suite, "StatRootDirectory");

    uint32_t size = 0, type = 0;
    int ret = vfs_stat("/", &size, &type);
    if (ret != 0) {
        ktest_case_fail(g_suite, "StatRootDirectory", "vfs_stat('/') returned -1");
        return -1;
    }
    if (type != VFS_TYPE_DIRECTORY) {
        ktest_case_fail(g_suite, "StatRootDirectory", "root is not a directory");
        return -1;
    }
    ktest_case_pass(g_suite, "StatRootDirectory");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_stat_file(void)
{
    ktest_case_begin(g_suite, "StatExistingFile");

    uint32_t size = 0, type = 0;
    int ret = vfs_stat("/test.txt", &size, &type);
    if (ret != 0) {
        ktest_case_fail(g_suite, "StatExistingFile", "vfs_stat('/test.txt') returned -1");
        return -1;
    }
    if (type != VFS_TYPE_FILE) {
        ktest_case_fail(g_suite, "StatExistingFile", "/test.txt is not a regular file");
        return -1;
    }
    if (size == 0) {
        ktest_case_fail(g_suite, "StatExistingFile", "/test.txt reported zero size");
        return -1;
    }
    ktest_case_pass(g_suite, "StatExistingFile");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_read_file(void)
{
    ktest_case_begin(g_suite, "ReadExistingFile");

    int fd = vfs_open("/test.txt", O_RDONLY, 0);
    if (fd < 0) {
        ktest_case_fail(g_suite, "ReadExistingFile", "vfs_open('/test.txt') returned -1");
        return -1;
    }

    uint8_t buf[64] = {0};
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    vfs_close(fd);

    if (n <= 0) {
        ktest_case_fail(g_suite, "ReadExistingFile", "vfs_read returned <= 0");
        return -1;
    }
    ktest_case_pass(g_suite, "ReadExistingFile");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_create_file(void)
{
    ktest_case_begin(g_suite, "CreateFile");

    /* Remove any leftover from a prior run */
    vfs_unlink(TEST_FILE_PATH);

    int fd = vfs_open(TEST_FILE_PATH, O_WRONLY | O_CREAT, VFS_DEFAULT_FILE_MODE);
    if (fd < 0) {
        ktest_case_fail(g_suite, "CreateFile", "vfs_open(O_CREAT) returned -1");
        return -1;
    }
    vfs_close(fd);

    uint32_t size = 0, type = 0;
    int ret = vfs_stat(TEST_FILE_PATH, &size, &type);
    if (ret != 0 || type != VFS_TYPE_FILE) {
        ktest_case_fail(g_suite, "CreateFile", "created file could not be stat'd back");
        return -1;
    }
    ktest_case_pass(g_suite, "CreateFile");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_write_file(void)
{
    ktest_case_begin(g_suite, "WriteAndReadBackFile");

    /* Write */
    int fd = vfs_open(TEST_FILE_PATH, O_WRONLY, 0);
    if (fd < 0) {
        ktest_case_fail(g_suite, "WriteAndReadBackFile", "vfs_open(O_WRONLY) returned -1");
        return -1;
    }
    int n = vfs_write(fd, TEST_WRITE_DATA, TEST_WRITE_SIZE);
    vfs_close(fd);
    if (n != TEST_WRITE_SIZE) {
        ktest_case_fail(g_suite, "WriteAndReadBackFile", "vfs_write returned the wrong byte count");
        return -1;
    }

    /* Read back and verify */
    fd = vfs_open(TEST_FILE_PATH, O_RDONLY, 0);
    if (fd < 0) {
        ktest_case_fail(g_suite, "WriteAndReadBackFile", "vfs_open(O_RDONLY) for readback returned -1");
        return -1;
    }
    uint8_t buf[32] = {0};
    int rn = vfs_read(fd, buf, sizeof(buf));
    vfs_close(fd);
    if (rn != TEST_WRITE_SIZE) {
        ktest_case_fail(g_suite, "WriteAndReadBackFile", "vfs_read returned the wrong byte count");
        return -1;
    }
    for (int i = 0; i < TEST_WRITE_SIZE; i++) {
        if (buf[i] != (uint8_t)TEST_WRITE_DATA[i]) {
            ktest_case_fail(g_suite, "WriteAndReadBackFile", "data read back from the file did not match what was written");
            return -1;
        }
    }
    ktest_case_pass(g_suite, "WriteAndReadBackFile");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_rejects_partial_write(void)
{
    ktest_case_begin(g_suite, "RejectsPartialWrite");

    int fd = vfs_open(TEST_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, VFS_DEFAULT_FILE_MODE);
    if (fd < 0) {
        ktest_case_fail(g_suite, "RejectsPartialWrite", "vfs_open for partial-write test returned -1");
        return -1;
    }

    ext2_vfs_test_force_write_size(TEST_WRITE_SIZE - 4);
    clear_errno();
    int n = vfs_write(fd, TEST_WRITE_DATA, TEST_WRITE_SIZE);
    int saved_errno = get_errno();
    vfs_close(fd);

    if (n != -1) {
        ktest_case_fail(g_suite, "RejectsPartialWrite", "vfs_write accepted a forced partial write");
        return -1;
    }
    if (saved_errno != THUNDEROS_EIO) {
        ktest_case_fail(g_suite, "RejectsPartialWrite", "forced partial write did not set EIO");
        return -1;
    }

    ktest_case_pass(g_suite, "RejectsPartialWrite");
    return 0;
}

/* ----------------------------------------------------------------------- */

static int test_vfs_delete_file(void)
{
    ktest_case_begin(g_suite, "DeleteFile");

    int ret = vfs_unlink(TEST_FILE_PATH);
    if (ret != 0) {
        ktest_case_fail(g_suite, "DeleteFile", "vfs_unlink returned -1");
        return -1;
    }

    uint32_t size = 0, type = 0;
    ret = vfs_stat(TEST_FILE_PATH, &size, &type);
    if (ret == 0) {
        ktest_case_fail(g_suite, "DeleteFile", "file still exists after unlink");
        return -1;
    }
    ktest_case_pass(g_suite, "DeleteFile");
    return 0;
}

/* ----------------------------------------------------------------------- */

void test_vfs_all(void)
{
    ktest_suite_t suite;

    ktest_suite_init(&suite, "Ext2VFS");
    ktest_suite_begin(&suite);
    g_suite = &suite;

    test_vfs_stat_root();
    test_vfs_stat_file();
    test_vfs_read_file();
    test_vfs_create_file();
    test_vfs_write_file();
    test_vfs_rejects_partial_write();
    test_vfs_delete_file();

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
