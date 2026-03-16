/*
 * errno Unit Tests
 *
 * Verifies the errno system: set_errno, get_errno, clear_errno,
 * RETURN_ERRNO macro, and thunderos_strerror.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "kernel/errno.h"
#include "kernel/kstring.h"

#define TEST_PASS(name) do { \
    hal_uart_puts("  [PASS] " name "\n"); \
    tests_passed++; \
    tests_total++; \
} while (0)

#define TEST_FAIL(name, msg) do { \
    hal_uart_puts("  [FAIL] " name ": " msg "\n"); \
    tests_total++; \
} while (0)

#define ASSERT(name, cond, msg) do { \
    if (cond) { TEST_PASS(name); } else { TEST_FAIL(name, msg); } \
} while (0)

/* Helper that uses RETURN_ERRNO macro */
static int helper_return_errno(int code)
{
    RETURN_ERRNO(code);
}

void test_errno(void) {
    hal_uart_puts("\n");
    hal_uart_puts("========================================\n");
    hal_uart_puts("  errno Unit Tests\n");
    hal_uart_puts("========================================\n\n");

    int tests_passed = 0;
    int tests_total = 0;

    /* Test 1: clear_errno sets errno to OK */
    set_errno(THUNDEROS_EINVAL);
    clear_errno();
    ASSERT("clear_errno sets errno to THUNDEROS_OK",
           get_errno() == THUNDEROS_OK,
           "errno was not cleared");

    /* Test 2: set_errno stores the code */
    set_errno(THUNDEROS_ENOMEM);
    ASSERT("set_errno stores error code",
           get_errno() == THUNDEROS_ENOMEM,
           "get_errno did not return THUNDEROS_ENOMEM");

    /* Test 3: set_errno returns -1 */
    clear_errno();
    int ret = set_errno(THUNDEROS_EINVAL);
    ASSERT("set_errno returns -1",
           ret == -1,
           "set_errno did not return -1");

    /* Test 4: errno holds last value set */
    set_errno(THUNDEROS_ENOENT);
    set_errno(THUNDEROS_EBADF);
    ASSERT("errno holds the last value set",
           get_errno() == THUNDEROS_EBADF,
           "errno was not updated to last set code");

    /* Test 5: RETURN_ERRNO macro sets errno and returns -1 */
    clear_errno();
    int macro_ret = helper_return_errno(THUNDEROS_EPERM);
    ASSERT("RETURN_ERRNO sets errno",
           get_errno() == THUNDEROS_EPERM,
           "RETURN_ERRNO did not set errno");
    ASSERT("RETURN_ERRNO returns -1",
           macro_ret == -1,
           "RETURN_ERRNO did not return -1");

    /* Test 6: clear_errno after set restores OK */
    set_errno(THUNDEROS_EIO);
    clear_errno();
    ASSERT("clear_errno restores THUNDEROS_OK after set",
           get_errno() == THUNDEROS_OK,
           "clear_errno did not restore OK");

    /* Test 7: thunderos_strerror returns non-NULL for known codes */
    const char *s = thunderos_strerror(THUNDEROS_ENOMEM);
    ASSERT("thunderos_strerror returns non-NULL",
           s != NULL,
           "thunderos_strerror returned NULL");

    /* Test 8: thunderos_strerror for THUNDEROS_OK returns "Success" */
    const char *ok_str = thunderos_strerror(THUNDEROS_OK);
    ASSERT("thunderos_strerror(OK) returns \"Success\"",
           ok_str != NULL && kstrcmp(ok_str, "Success") == 0,
           "thunderos_strerror(OK) did not return \"Success\"");

    /* Test 9: thunderos_strerror for EINVAL returns non-empty string */
    const char *einval_str = thunderos_strerror(THUNDEROS_EINVAL);
    ASSERT("thunderos_strerror(EINVAL) returns non-empty string",
           einval_str != NULL && einval_str[0] != '\0',
           "thunderos_strerror(EINVAL) returned empty string");

    /* Test 10: errno survives set/clear cycle without corruption */
    for (int i = 1; i <= 10; i++) {
        set_errno(i);
        if (get_errno() != i) {
            TEST_FAIL("errno set/clear cycle (no corruption)", "mismatch on iteration");
            goto done;
        }
        clear_errno();
        if (get_errno() != THUNDEROS_OK) {
            TEST_FAIL("errno set/clear cycle (no corruption)", "clear failed on iteration");
            goto done;
        }
    }
    TEST_PASS("errno set/clear cycle (no corruption)");

done:
    clear_errno();

    hal_uart_puts("\n");
    hal_uart_puts("errno tests: ");
    kprint_dec(tests_passed);
    hal_uart_puts("/");
    kprint_dec(tests_total);
    hal_uart_puts(" passed\n\n");
}

#endif /* ENABLE_KERNEL_TESTS */
