/*
 * Synchronization Primitive Unit Tests
 *
 * Covers mutex, rwlock, and condition-variable behavior using the existing
 * kernel test harness and wait-queue wake paths.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/condvar.h"
#include "kernel/errno.h"
#include "kernel/kstring.h"
#include "kernel/mutex.h"
#include "kernel/process.h"
#include "kernel/rwlock.h"
#include "mm/kmalloc.h"
#include "tests/structured_test_kernel.h"

static ktest_suite_t *g_suite = NULL;

#define ASSERT(name, cond, msg) KTEST_ASSERT((*g_suite), (name), (cond), (msg))

static int enqueue_fake_waiter(wait_queue_t *queue, struct process *proc) {
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
       if (!entry || !proc) {
        if (entry) {
            kfree(entry);
        }
        return -1;
    }

    entry->proc = proc;
    entry->next = NULL;

    if (queue->tail) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->count++;
    return 0;
}

static void release_fake_waiters(wait_queue_t *queue) {
    wait_queue_entry_t *entry = queue->head;
    while (entry) {
        wait_queue_entry_t *next = entry->next;
        kfree(entry);
        entry = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}

static void test_mutex_primitives(void) {
    mutex_t mutex = MUTEX_INIT;
    struct process *current = process_current();
       struct process fake_waiter;

       kmemset(&fake_waiter, 0, sizeof(fake_waiter));
       fake_waiter.state = PROC_READY;

       ASSERT("StartsUnlocked",
           mutex_is_locked(&mutex) == 0,
           "expected mutex to start unlocked");

    clear_errno();
    ASSERT("TrylockSucceedsOnUnlockedMutex",
           mutex_trylock(&mutex) == 0,
           "expected trylock to acquire unlocked mutex");
    ASSERT("TrylockRecordsOwnerPid",
           current != NULL && mutex.owner_pid == current->pid,
           "owner pid not set to current process");

    clear_errno();
    ASSERT("TrylockFailsWhenAlreadyLocked",
           mutex_trylock(&mutex) == -1 && get_errno() == THUNDEROS_EBUSY,
           "expected EBUSY on second trylock");

    ASSERT("CanSeedWaitQueue",
           enqueue_fake_waiter(&mutex.waiters, &fake_waiter) == 0,
           "failed to seed mutex wait queue");
    ASSERT("WaitQueueCountIncrements",
           wait_queue_count(&mutex.waiters) == 1,
           "expected one queued mutex waiter");

    mutex_unlock(&mutex);
    ASSERT("UnlockClearsLockState",
           mutex_is_locked(&mutex) == 0 && mutex.owner_pid == -1,
           "expected mutex to be unlocked after unlock");
    ASSERT("UnlockDrainsOneWaiter",
           wait_queue_count(&mutex.waiters) == 0,
           "expected mutex wake path to remove seeded waiter");
}

static void test_rwlock_primitives(void) {
    rwlock_t lock = RWLOCK_INIT;
       struct process fake_writer;
       struct process fake_reader_a;
       struct process fake_reader_b;

       kmemset(&fake_writer, 0, sizeof(fake_writer));
       kmemset(&fake_reader_a, 0, sizeof(fake_reader_a));
       kmemset(&fake_reader_b, 0, sizeof(fake_reader_b));
       fake_writer.state = PROC_READY;
       fake_reader_a.state = PROC_READY;
       fake_reader_b.state = PROC_READY;

    clear_errno();
    ASSERT("ReadTrylockSucceedsOnFreshLock",
           rwlock_read_trylock(&lock) == 0,
           "expected first reader to acquire rwlock");
    ASSERT("ReadTrylockAllowsSecondReader",
           rwlock_read_trylock(&lock) == 0 && rwlock_reader_count(&lock) == 2,
           "expected second reader to share rwlock");

    clear_errno();
    ASSERT("WriteTrylockFailsWhileReadersHoldLock",
           rwlock_write_trylock(&lock) == -1 && get_errno() == THUNDEROS_EBUSY,
           "expected writer trylock to fail while readers active");

    lock.writers_waiting = 1;
    ASSERT("CanSeedWriterQueue",
           enqueue_fake_waiter(&lock.writer_queue, &fake_writer) == 0,
           "failed to seed rwlock writer queue");
    rwlock_read_unlock(&lock);
    ASSERT("ReadUnlockDecrementsReaderCount",
           rwlock_reader_count(&lock) == 1,
           "expected reader count to drop to one");
    rwlock_read_unlock(&lock);
    ASSERT("LastReaderWakesOneWriter",
           rwlock_reader_count(&lock) == 0 && wait_queue_count(&lock.writer_queue) == 0,
           "expected last reader unlock to drain one writer waiter");

    lock.writers_waiting = 0;
    clear_errno();
    ASSERT("WriteTrylockSucceedsAfterReadersRelease",
           rwlock_write_trylock(&lock) == 0 && rwlock_is_write_locked(&lock) == 1,
           "expected writer to acquire rwlock after readers release");

    clear_errno();
    ASSERT("ReadTrylockFailsWhileWriterHoldsLock",
           rwlock_read_trylock(&lock) == -1 && get_errno() == THUNDEROS_EBUSY,
           "expected reader trylock to fail while writer active");

    ASSERT("CanSeedReaderQueue",
           enqueue_fake_waiter(&lock.reader_queue, &fake_reader_a) == 0 &&
           enqueue_fake_waiter(&lock.reader_queue, &fake_reader_b) == 0,
           "failed to seed rwlock reader queue");
    rwlock_write_unlock(&lock);
    ASSERT("WriteUnlockClearsWriterState",
           rwlock_is_write_locked(&lock) == 0,
           "expected writer state to clear on unlock");
    ASSERT("WriteUnlockWakesQueuedReaders",
           wait_queue_count(&lock.reader_queue) == 0,
           "expected queued readers to drain on writer unlock");

    lock.writers_waiting = 1;
    clear_errno();
    ASSERT("ReadTrylockRespectsWaitingWriterPriority",
           rwlock_read_trylock(&lock) == -1 && get_errno() == THUNDEROS_EBUSY,
           "expected waiting writer to block new readers");
}

static void test_condvar_primitives(void) {
    condvar_t cond = CONDVAR_INIT;
       struct process fake_waiter_a;
       struct process fake_waiter_b;
       struct process fake_waiter_c;

       kmemset(&fake_waiter_a, 0, sizeof(fake_waiter_a));
       kmemset(&fake_waiter_b, 0, sizeof(fake_waiter_b));
       kmemset(&fake_waiter_c, 0, sizeof(fake_waiter_c));
       fake_waiter_a.state = PROC_READY;
       fake_waiter_b.state = PROC_READY;
       fake_waiter_c.state = PROC_READY;

    cond_init(&cond);
    ASSERT("StartsWithEmptyQueue",
           wait_queue_empty(&cond.waiters),
           "expected condvar wait queue to start empty");

    ASSERT("CanSeedWaitQueue",
           enqueue_fake_waiter(&cond.waiters, &fake_waiter_a) == 0,
           "failed to seed condvar wait queue");
    cond_signal(&cond);
    ASSERT("SignalDrainsOneWaiter",
           wait_queue_count(&cond.waiters) == 0,
           "expected cond_signal to remove a queued waiter");

    ASSERT("CanSeedBroadcastQueue",
           enqueue_fake_waiter(&cond.waiters, &fake_waiter_b) == 0 &&
           enqueue_fake_waiter(&cond.waiters, &fake_waiter_c) == 0,
           "failed to seed condvar broadcast queue");
    cond_broadcast(&cond);
    ASSERT("BroadcastDrainsAllWaiters",
           wait_queue_count(&cond.waiters) == 0,
           "expected cond_broadcast to remove all queued waiters");

    release_fake_waiters(&cond.waiters);
}

void test_sync_primitives_all(void) {
       ktest_suite_t suite;

       ktest_suite_init(&suite, "SyncPrimitives");
       ktest_suite_begin(&suite);

       g_suite = &suite;

       test_mutex_primitives();
       test_rwlock_primitives();
       test_condvar_primitives();

       ktest_suite_end(&suite);
}

#endif /* ENABLE_KERNEL_TESTS */