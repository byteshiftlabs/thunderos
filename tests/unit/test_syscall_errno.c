/*
 * Syscall errno regression tests
 *
 * Covers boundary errno behavior that was fixed during the codebase
 * polishing audit.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/syscall.h"
#include "kernel/errno.h"
#include "kernel/process.h"
#include "kernel/kstring.h"
#include "hal/hal_uart.h"
#include "trap.h"

/* Forward declaration: implemented in kernel/core/process.c */
void process_set_current(struct process *proc);
uint64_t sys_execve_with_frame(struct trap_frame *tf, const char *path, const char *argv[], const char *envp[]);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    do { \
        hal_uart_puts("Test: "); \
        hal_uart_puts(name); \
        hal_uart_puts("... "); \
    } while (0)

#define TEST_PASS() \
    do { \
        hal_uart_puts("PASS\n"); \
        tests_passed++; \
    } while (0)

#define TEST_FAIL(msg) \
    do { \
        hal_uart_puts("FAIL - "); \
        hal_uart_puts(msg); \
        hal_uart_puts("\n"); \
        tests_failed++; \
        return; \
    } while (0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
        } \
    } while (0)

static void test_getuid_clears_errno(void) {
    TEST_START("sys_getuid clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_getuid() == 0, "sys_getuid should return root for init");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_gettty_clears_errno(void) {
    TEST_START("sys_gettty clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_gettty() == 0, "sys_gettty should return init terminal");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_settty_clears_errno(void) {
    TEST_START("sys_settty clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_settty(0) == 0, "sys_settty should accept tty 0");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_getcwd_invalid_pointer_sets_efault(void) {
    TEST_START("sys_getcwd reports EFAULT for invalid user buffer");

    clear_errno();
    ASSERT_TRUE(sys_getcwd((char *)USER_CODE_BASE, 16) == 0, "sys_getcwd should return NULL on failure");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT, "sys_getcwd should set EFAULT");

    TEST_PASS();
}

static void test_getdents_invalid_pointer_sets_efault(void) {
    TEST_START("sys_getdents reports EFAULT for invalid user buffer");

    clear_errno();
    ASSERT_TRUE(sys_getdents(3, (void *)USER_CODE_BASE, 512) == (uint64_t)-1, "sys_getdents should fail for invalid user buffer");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT, "sys_getdents should set EFAULT before fd checks");

    TEST_PASS();
}

static void test_chdir_invalid_pointer_sets_efault(void) {
    TEST_START("sys_chdir reports EFAULT for invalid path pointer");

    clear_errno();
    ASSERT_TRUE(sys_chdir((const char *)USER_CODE_BASE) == (uint64_t)-1, "sys_chdir should fail for invalid path pointer");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT, "sys_chdir should set EFAULT");

    TEST_PASS();
}

static void test_execve_invalid_pointer_sets_efault(void) {
    TEST_START("sys_execve_with_frame reports EFAULT for invalid path pointer");

    struct trap_frame tf;
    kmemset(&tf, 0, sizeof(tf));

    clear_errno();
    ASSERT_TRUE(sys_execve_with_frame(&tf, (const char *)USER_CODE_BASE, NULL, NULL) == (uint64_t)-1,
                "sys_execve_with_frame should fail for invalid path pointer");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT, "sys_execve_with_frame should set EFAULT");

    TEST_PASS();
}

static void test_waitpid_invalid_status_pointer_sets_efault(void) {
    TEST_START("sys_waitpid reports EFAULT for invalid status pointer");

    clear_errno();
    ASSERT_TRUE(sys_waitpid(-1, (int *)USER_CODE_BASE, 0) == (uint64_t)-1,
                "sys_waitpid should fail for invalid status pointer");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT,
                "sys_waitpid should set EFAULT before child-state checks");

    TEST_PASS();
}

static void test_getpid_clears_errno(void) {
    TEST_START("sys_getpid clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_getpid() == 0, "sys_getpid should return init pid");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_sbrk_zero_clears_errno(void) {
    TEST_START("sys_sbrk clears stale errno on zero increment");

    set_errno(THUNDEROS_EIO);
    (void)sys_sbrk(0);
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_sleep_zero_clears_errno(void) {
    TEST_START("sys_sleep clears stale errno on zero-duration success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_sleep(0) == 0, "sys_sleep should succeed for zero duration");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_yield_clears_errno(void) {
    TEST_START("sys_yield clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_yield() == 0, "sys_yield should succeed");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

static void test_gettime_clears_errno(void) {
    TEST_START("sys_gettime clears stale errno on success");

    set_errno(THUNDEROS_EIO);
    (void)sys_gettime();
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    TEST_PASS();
}

void run_syscall_errno_tests(void) {
    struct process *saved_process = process_current();

    tests_passed = 0;
    tests_failed = 0;

    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("   Syscall Errno Regression Tests\n");
    hal_uart_puts("========================================\n\n");

    process_set_current(process_get(0));

    test_getuid_clears_errno();
    test_gettty_clears_errno();
    test_settty_clears_errno();
    test_getcwd_invalid_pointer_sets_efault();
    test_getdents_invalid_pointer_sets_efault();
    test_chdir_invalid_pointer_sets_efault();
    test_execve_invalid_pointer_sets_efault();
    test_waitpid_invalid_status_pointer_sets_efault();
    test_getpid_clears_errno();
    test_sbrk_zero_clears_errno();
    test_sleep_zero_clears_errno();
    test_yield_clears_errno();
    test_gettime_clears_errno();

    process_set_current(saved_process);

    hal_uart_puts("\nSummary: ");
    if (tests_failed == 0) {
        hal_uart_puts("ALL TESTS PASSED");
    } else {
        hal_uart_puts("SOME TESTS FAILED");
    }
    hal_uart_puts("\n\n");
}

#endif /* ENABLE_KERNEL_TESTS */
