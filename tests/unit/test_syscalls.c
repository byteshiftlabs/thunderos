/*
 * Syscall Unit Tests
 *
 * Tests the system call interface: getpid, gettime, getuid, getgid,
 * kill with invalid arguments, and the syscall dispatcher.
 *
 * These tests run after process_init() and scheduler_init() so that
 * current_process is valid (init process, pid 0).
 *
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/process.h"
#include "kernel/constants.h"
#include "kernel/scheduler.h"
#include "kernel/syscall.h"
#include "kernel/errno.h"
#include "kernel/signal.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

static void syscall_test_child_entry(void *arg) {
       (void)arg;
}

void test_syscalls_all(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "Syscalls");
    ktest_suite_begin(&suite);

    clear_errno();
    uint64_t pid = sys_getpid();
    ASSERT(suite, "GetpidReturnsInitPid",
           pid == 0,
           "expected pid 0 for init process");
    ASSERT(suite, "GetpidClearsErrnoOnSuccess",
           get_errno() == THUNDEROS_OK,
           "errno should be clear after successful getpid");

    clear_errno();
    uint64_t t0 = sys_gettime();
    uint64_t t1 = sys_gettime();
    ASSERT(suite, "GettimeReturnsNonNegativeValue",
           (int64_t)t0 >= 0,
           "gettime returned negative");
    ASSERT(suite, "GettimeIsNonDecreasing",
           t1 >= t0,
           "second gettime call returned smaller value");
    ASSERT(suite, "GettimeClearsErrnoOnSuccess",
           get_errno() == THUNDEROS_OK,
           "errno should be clear after successful gettime");

    clear_errno();
    uint64_t uid = sys_getuid();
    ASSERT(suite, "GetuidReturnsRootForInit",
           uid == 0,
           "init process should have uid 0");

    clear_errno();
    uint64_t gid = sys_getgid();
    ASSERT(suite, "GetgidReturnsRootForInit",
           gid == 0,
           "init process should have gid 0");

    clear_errno();
    uint64_t ppid = sys_getppid();
    ASSERT(suite, "GetppidDoesNotCrashOnInit",
           (int64_t)ppid >= -1,
           "getppid returned unexpected value");

    clear_errno();
    uint64_t kill_ret = sys_kill(-999, SIGTERM);
    ASSERT(suite, "KillWithInvalidPidReturnsError",
           (int64_t)kill_ret == -1,
           "expected -1 for invalid kill pid");
    ASSERT(suite, "KillWithInvalidPidSetsErrno",
           get_errno() != THUNDEROS_OK,
           "errno should be set after failed kill");

    clear_errno();
    kill_ret = sys_kill(0, -1);
    ASSERT(suite, "KillWithInvalidSignalReturnsError",
           (int64_t)kill_ret == -1,
           "expected -1 for invalid signal number");

    clear_errno();
    uint64_t unknown = syscall_handler(9999, 0, 0, 0, 0, 0, 0);
    ASSERT(suite, "UnknownSyscallReturnsMinusOne",
           unknown == (uint64_t)-1,
           "expected (uint64_t)-1 for unknown syscall");

    struct process *stopped_child = process_create("waitpid-stop", syscall_test_child_entry, NULL);
    ASSERT(suite, "WaitpidStoppedChildCreated",
           stopped_child != NULL,
           "expected test child process to be created");

    if (stopped_child) {
        scheduler_dequeue(stopped_child);
        stopped_child->state = PROC_STOPPED;
        stopped_child->exit_code = (SIGTSTP << 8) | WAIT_STOPPED_INDICATOR;

        clear_errno();
        uint64_t wait_ret = sys_waitpid(-1, (int *)KERNEL_SPACE_START, 0);
        ASSERT(suite, "WaitpidStoppedChildRejectsInvalidStatusPointer",
               wait_ret == (uint64_t)-1,
               "expected waitpid to reject invalid stopped-child status pointer");
        ASSERT(suite, "WaitpidStoppedChildSetsEfault",
               get_errno() == THUNDEROS_EFAULT,
               "expected EFAULT for invalid stopped-child status pointer");
        ASSERT(suite, "WaitpidStoppedChildStatePreservedOnFault",
               stopped_child->state == PROC_STOPPED &&
               stopped_child->exit_code == ((SIGTSTP << 8) | WAIT_STOPPED_INDICATOR),
               "failed waitpid must not consume stopped-child state");

        process_free(stopped_child);
    }

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */
