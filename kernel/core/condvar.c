/**
 * @file condvar.c
 * @brief Condition variable implementation for ThunderOS
 *
 * Implements condition variables for coordinating threads/processes.
 * Condition variables work with mutexes to provide wait/signal semantics.
 */

#include "kernel/condvar.h"
#include "kernel/mutex.h"
#include "kernel/wait_queue.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/errno.h"
#include "mm/kmalloc.h"
#include "arch/interrupt.h"

/**
 * @brief Initialize a condition variable
 *
 * Sets up an empty wait queue for processes waiting on this condition.
 *
 * @param cv Pointer to condition variable to initialize
 */
void cond_init(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    wait_queue_init(&cv->waiters);
}

/**
 * @brief Wait on a condition variable (blocking)
 *
 * This is the core condition variable operation. It performs the following
 * atomic sequence:
 * 1. Add the calling process to the CV wait queue
 * 2. Unlock the provided mutex
 * 3. Put the calling process to sleep
 * 4. When awakened, re-acquire the mutex before returning
 *
 * Steps 1-3 execute with interrupts disabled so that no signal can arrive
 * between unlocking the mutex and sleeping (the "lost wakeup" problem).
 *
 * The wait queue logic is inlined rather than calling wait_queue_sleep()
 * because we need the enqueue to happen BEFORE the mutex unlock, but all
 * under a single interrupt-disable region.
 *
 * @param cv Pointer to condition variable to wait on
 * @param mutex Pointer to mutex associated with this condition (must be locked)
 */
void cond_wait(condvar_t *cv, mutex_t *mutex) {
    if (!cv || !mutex) {
        return;
    }
    
    struct process *current = process_current();
    if (!current) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /*
     * Step 1: Enqueue ourselves on the CV wait queue FIRST.
     * After this point, any cond_signal/cond_broadcast will find us.
     */
    wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
    if (!entry) {
        interrupt_restore(flags);
        return;
    }
    entry->proc = current;
    entry->next = NULL;
    
    if (cv->waiters.tail) {
        cv->waiters.tail->next = entry;
        cv->waiters.tail = entry;
    } else {
        cv->waiters.head = entry;
        cv->waiters.tail = entry;
    }
    cv->waiters.count++;
    
    /*
     * Step 2: Unlock the mutex.
     * We're already on the wait queue, so any signal arriving after this
     * unlock will find and wake us — no lost wakeup possible.
     */
    mutex->locked = MUTEX_UNLOCKED;
    mutex->owner_pid = -1;
    wait_queue_wake(&mutex->waiters);
    
    /*
     * Step 3: Mark ourselves as sleeping and yield.
     * Still under the same interrupt-disable region.
     */
    current->state = PROC_SLEEPING;
    scheduler_dequeue(current);
    
    interrupt_restore(flags);
    schedule();
    
    /*
     * We've been woken up (entry was freed by the wake function).
     * Re-acquire the mutex before returning to the caller.
     * The caller should re-check the condition in a while loop.
     */
    mutex_lock(mutex);
}

/**
 * @brief Signal one waiting process
 *
 * Wakes up one process waiting on the condition variable. If no processes
 * are waiting, this is a no-op. The awakened process will re-acquire the
 * mutex before cond_wait() returns.
 *
 * Best practice: Call this while holding the associated mutex to ensure
 * atomicity of condition changes and signaling.
 *
 * @param cv Pointer to condition variable to signal
 */
void cond_signal(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wake one waiting process (if any) */
    wait_queue_wake_one(&cv->waiters);
    
    interrupt_restore(flags);
}

/**
 * @brief Broadcast to all waiting processes
 *
 * Wakes up all processes waiting on the condition variable. All awakened
 * processes will compete to re-acquire the mutex. This is typically used
 * when a condition change may satisfy multiple waiters.
 *
 * Best practice: Call this while holding the associated mutex to ensure
 * atomicity of condition changes and broadcasting.
 *
 * @param cv Pointer to condition variable to broadcast
 */
void cond_broadcast(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    uint64_t flags = interrupt_save_disable();
    
    /* Wake all waiting processes */
    wait_queue_wake(&cv->waiters);
    
    interrupt_restore(flags);
}

/**
 * @brief Destroy a condition variable
 *
 * Cleans up a condition variable. In our implementation, this is currently
 * a no-op since we don't dynamically allocate resources. However, it's
 * good practice to call this for symmetry with cond_init().
 *
 * Warning: No processes should be waiting on the condition variable when
 * this is called, or they may never wake up.
 *
 * @param cv Pointer to condition variable to destroy
 */
void cond_destroy(condvar_t *cv) {
    if (!cv) {
        return;
    }
    
    /* 
     * In a more sophisticated implementation, we might:
     * - Check that the wait queue is empty
     * - Wake any remaining waiters with an error
     * - Free any dynamically allocated resources
     * 
     * For now, this is a no-op.
     */
}
