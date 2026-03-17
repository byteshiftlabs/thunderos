/*
 * Wait Queue Unit Tests
 *
 * Verifies that wait_queue_sleep handles allocation failure safely and does not
 * modify queue state or force a schedule before the process is enqueued.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/errno.h"
#include "kernel/process.h"
#include "kernel/wait_queue.h"
#include "tests/structured_test_kernel.h"

#define ASSERT(suite, name, cond, msg) KTEST_ASSERT((suite), (name), (cond), (msg))

void test_wait_queue_all(void) {
       ktest_suite_t suite;
       ktest_suite_init(&suite, "WaitQueue");
       ktest_suite_begin(&suite);

    wait_queue_t wq = WAIT_QUEUE_INIT;

    clear_errno();
    int ret = wait_queue_sleep(NULL);
       ASSERT(suite, "SleepRejectsNullQueue",
           ret == -1,
           "expected wait_queue_sleep(NULL) to fail");
       ASSERT(suite, "SleepSetsEinvalForNullQueue",
           get_errno() == THUNDEROS_EINVAL,
           "expected THUNDEROS_EINVAL for NULL queue");

    struct process *current = process_current();
       ASSERT(suite, "CurrentProcessAvailableForTests",
           current != NULL,
           "expected a current process during kernel tests");

    if (current) {
        proc_state_t state_before = current->state;

        wait_queue_test_force_alloc_failure(1);
        clear_errno();
        ret = wait_queue_sleep(&wq);

        ASSERT(suite, "SleepReturnsEnomemOnForcedAllocationFailure",
               ret == -1,
               "expected ENOMEM failure from forced wait queue allocation failure");
        ASSERT(suite, "SleepSetsEnomemOnForcedAllocationFailure",
               get_errno() == THUNDEROS_ENOMEM,
               "expected THUNDEROS_ENOMEM on allocation failure");
        ASSERT(suite, "QueueRemainsEmptyOnAllocationFailure",
               wait_queue_count(&wq) == 0 && wait_queue_empty(&wq),
               "queue state changed despite failed enqueue");
        ASSERT(suite, "ProcessStateUnchangedOnAllocationFailure",
               current->state == state_before,
               "process state changed on failed wait queue sleep");
    }

    ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */