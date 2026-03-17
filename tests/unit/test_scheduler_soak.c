/*
 * Scheduler Soak Tests
 *
 * Exercises repeated context switching among kernel-mode worker processes to
 * provide repeatable scheduler churn coverage under the kernel test build.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "hal/hal_uart.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/syscall.h"
#include "tests/structured_test_kernel.h"

#define SCHED_SOAK_WORKERS     6
#define SCHED_SOAK_ITERATIONS  24
#define SCHED_SOAK_BUDGET      4096

typedef struct {
    int slot;
    int iterations;
} scheduler_soak_arg_t;

static volatile int g_scheduler_ticks[SCHED_SOAK_WORKERS];
static volatile int g_scheduler_done[SCHED_SOAK_WORKERS];
static scheduler_soak_arg_t g_scheduler_args[SCHED_SOAK_WORKERS];

static void scheduler_soak_worker(void *arg) {
    scheduler_soak_arg_t *worker_arg = (scheduler_soak_arg_t *)arg;

    for (int iteration = 0; iteration < worker_arg->iterations; iteration++) {
        g_scheduler_ticks[worker_arg->slot]++;
        scheduler_yield();
    }

    g_scheduler_done[worker_arg->slot] = 1;
    process_exit(0);
}

void test_scheduler_soak_all(void) {
    ktest_suite_t suite;
    struct process *workers[SCHED_SOAK_WORKERS] = {0};
    int statuses[SCHED_SOAK_WORKERS] = {0};
    int all_done = 0;

    ktest_suite_init(&suite, "SchedulerSoak");
    ktest_suite_begin(&suite);

    for (int i = 0; i < SCHED_SOAK_WORKERS; i++) {
        g_scheduler_ticks[i] = 0;
        g_scheduler_done[i] = 0;
        g_scheduler_args[i].slot = i;
        g_scheduler_args[i].iterations = SCHED_SOAK_ITERATIONS;

        workers[i] = process_create("sched_soak", scheduler_soak_worker, &g_scheduler_args[i]);
        KTEST_ASSERT(suite, "CreatesWorkerProcess",
                 workers[i] != NULL,
                 "process_create returned NULL during scheduler soak setup");
    }

    for (int budget = 0; budget < SCHED_SOAK_BUDGET; budget++) {
        all_done = 1;
        for (int i = 0; i < SCHED_SOAK_WORKERS; i++) {
            if (!g_scheduler_done[i]) {
                all_done = 0;
                break;
            }
        }

        if (all_done) {
            break;
        }

        scheduler_yield();
    }

    KTEST_ASSERT(suite, "WorkersCompleteWithinBudget",
                 all_done,
                 "scheduler soak workers did not all finish within the expected yield budget");

    for (int i = 0; i < SCHED_SOAK_WORKERS; i++) {
        KTEST_ASSERT(suite, "EachWorkerRanExpectedIterations",
                 g_scheduler_ticks[i] == SCHED_SOAK_ITERATIONS,
                 "a worker did not receive the expected number of scheduled turns");
    }

    for (int i = 0; i < SCHED_SOAK_WORKERS; i++) {
        if (!workers[i]) {
            continue;
        }

        KTEST_ASSERT(suite, "WorkerExitedToZombie",
                 workers[i]->state == PROC_ZOMBIE,
                 "worker did not reach zombie state after completing scheduler soak iterations");

        if (workers[i]->state == PROC_ZOMBIE) {
            int waited_pid = (int)sys_waitpid(workers[i]->pid, &statuses[i], 0);
            KTEST_ASSERT(suite, "ParentReapsWorker",
                         waited_pid == workers[i]->pid,
                         "sys_waitpid did not reap the expected scheduler soak worker");
        }
    }

    ktest_suite_end(&suite);
}

#endif