/*
 * errno Unit Tests
 *
 * Verifies the errno system: set_errno, get_errno, clear_errno,
 * RETURN_ERRNO macro, and thunderos_strerror.
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/errno.h"
#include "kernel/kstring.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

/* Helper that uses RETURN_ERRNO macro */
static int helper_return_errno(int code)
{
    RETURN_ERRNO(code);
}

void test_errno(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "Errno");
    ktest_suite_begin(&suite);

    set_errno(THUNDEROS_EINVAL);
    clear_errno();
    ASSERT(suite, "ClearErrnoSetsOk",
           get_errno() == THUNDEROS_OK,
           "errno was not cleared");

    set_errno(THUNDEROS_ENOMEM);
    ASSERT(suite, "SetErrnoStoresCode",
           get_errno() == THUNDEROS_ENOMEM,
           "get_errno did not return THUNDEROS_ENOMEM");

    clear_errno();
    int ret = set_errno(THUNDEROS_EINVAL);
    ASSERT(suite, "SetErrnoReturnsMinusOne",
           ret == -1,
           "set_errno did not return -1");

    set_errno(THUNDEROS_ENOENT);
    set_errno(THUNDEROS_EBADF);
    ASSERT(suite, "ErrnoTracksLastValueSet",
           get_errno() == THUNDEROS_EBADF,
           "errno was not updated to last set code");

    clear_errno();
    int macro_ret = helper_return_errno(THUNDEROS_EPERM);
    ASSERT(suite, "ReturnErrnoMacroSetsErrno",
           get_errno() == THUNDEROS_EPERM,
           "RETURN_ERRNO did not set errno");
    ASSERT(suite, "ReturnErrnoMacroReturnsMinusOne",
           macro_ret == -1,
           "RETURN_ERRNO did not return -1");

    set_errno(THUNDEROS_EIO);
    clear_errno();
    ASSERT(suite, "ClearErrnoRestoresOkAfterSet",
           get_errno() == THUNDEROS_OK,
           "clear_errno did not restore OK");

    const char *s = thunderos_strerror(THUNDEROS_ENOMEM);
    ASSERT(suite, "StrerrorReturnsNonNullForKnownCode",
           s != NULL,
           "thunderos_strerror returned NULL");

    const char *ok_str = thunderos_strerror(THUNDEROS_OK);
    ASSERT(suite, "StrerrorOkReturnsSuccess",
           ok_str != NULL && kstrcmp(ok_str, "Success") == 0,
           "thunderos_strerror(OK) did not return \"Success\"");

    const char *einval_str = thunderos_strerror(THUNDEROS_EINVAL);
    ASSERT(suite, "StrerrorEinvalReturnsNonEmptyString",
           einval_str != NULL && einval_str[0] != '\0',
           "thunderos_strerror(EINVAL) returned empty string");

    int cycle_ok = 1;
    for (int i = 1; i <= 10; i++) {
        set_errno(i);
        if (get_errno() != i) {
            cycle_ok = 0;
            break;
        }
        clear_errno();
        if (get_errno() != THUNDEROS_OK) {
            cycle_ok = 0;
            break;
        }
    }
    ASSERT(suite, "ErrnoSetClearCycleIsStable", cycle_ok,
           "errno set/clear cycle became inconsistent");

    clear_errno();

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
