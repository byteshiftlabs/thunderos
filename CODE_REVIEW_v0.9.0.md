# ThunderOS v0.9.0 — Code Review

**Reviewer:** Silvanus Trold  
**Date:** 2025-07-13  
**Scope:** Full kernel audit — boot, memory management, process/scheduler, synchronization, syscalls, signals, ELF loader, assembly  
**Verdict:** The architecture is ambitious and the design direction is sound. But this kernel has structural correctness bugs that will bite you the moment you stress-test it. Let's fix them.

---

## Summary

| Severity | Count | Description |
|----------|-------|-------------|
| **BLOCKER** | 5 | Will cause crashes, memory corruption, or deadlocks under normal use |
| **SERIOUS** | 14 | Incorrect behavior, race conditions, resource leaks under concurrency |
| **MINOR** | 10 | Style issues, stale constants, code duplication |

---

## BLOCKER

### B1 — Physical Memory Leak on Process Exit

**Files:** `paging.c` (`free_page_table_recursive`), `process.c` (`process_exit`)

`free_page_table_recursive` only frees intermediate page table pages. It explicitly does NOT free leaf pages (the actual user code, data, stack, and heap pages). The comment even says so:

```c
// NOTE: This function frees the page table structure itself.
// It does NOT free the actual data pages mapped by the page table.
```

Every `process_exit` → `free_page_table` leaks every physical page the process ever mapped. After enough `fork`/`exec`/`exit` cycles, the PMM will be exhausted and the system locks up.

**Fix:** Walk the page table and `pmm_free_page` every leaf PTE's physical address before freeing page table pages. Guard against freeing kernel-mapped pages (identity-mapped region).

---

### B2 — Lost Wakeup in `cond_wait` (Unlock-and-Sleep Not Atomic)

**File:** `condvar.c`, `cond_wait`

The critical sequence is:

```c
uint64_t flags = interrupt_save_disable();
mutex->locked = MUTEX_UNLOCKED;          // 1. Unlock mutex
wait_queue_wake(&mutex->waiters);         // 2. Wake mutex waiters
interrupt_restore(flags);                 // 3. RE-ENABLE INTERRUPTS ←
wait_queue_sleep(&cv->waiters);           // 4. Sleep on condvar
```

Between steps 3 and 4, interrupts are enabled and the process is NOT yet on the condvar wait queue. If another process calls `cond_signal` in this window, it finds an empty wait queue and does nothing. The signaler's wakeup is lost. The waiter then enters `wait_queue_sleep`, adds itself to the queue, and sleeps **forever**.

This is the textbook lost-wakeup bug that condition variables are specifically designed to prevent.

**Fix:** Don't restore interrupts before sleeping. Add the process to the condvar wait queue while interrupts are disabled, then mark as SLEEPING and yield — all atomically. This likely requires refactoring `wait_queue_sleep` to accept a "prepare" callback or splitting it into `wait_queue_enqueue` + `schedule`.

---

### B3 — Signal Handling Corrupts User Registers (No Signal Frame)

**File:** `signal.c`, `signal_handle_with_frame`

```c
trap_frame->ra = trap_frame->sepc;   // Clobber ra
trap_frame->sepc = (unsigned long)handler;
trap_frame->a0 = signum;
```

This redirects execution to the signal handler, but:
1. **`ra` is clobbered** — if the interrupted code was using `ra`, that value is lost
2. **No signal frame** is pushed on the user stack — all registers except `a0` contain the interrupted code's values, which the handler may corrupt
3. **No `sigreturn` mechanism** — the handler does `ret`, which jumps to the original `sepc` via `ra`, but if the handler called ANY function, `ra` is overwritten → the process crashes or enters an infinite loop
4. **Only one signal delivered per trap return** is correct, but the clobbered state accumulates

Any user-defined signal handler that does more than a single instruction will corrupt the process.

**Fix:** Implement a proper signal frame. Before entering the handler: push the full trap frame onto the user stack, set `ra` to a `sigreturn` trampoline address, set `sepc` to the handler. The `sigreturn` syscall restores the original trap frame from the user stack.

---

### B4 — `kmalloc_aligned` Returns Unaligned Memory

**File:** `kmalloc.c`, `kmalloc_aligned`

```c
void *kmalloc_aligned(size_t size, size_t alignment) {
    (void)alignment;
    return kmalloc(size);  // "page-aligned because we allocate full pages"
}
```

But `kmalloc` prepends a `kmalloc_header_t` (HEADER_SIZE bytes) before the returned pointer:
```c
header->magic = KMALLOC_MAGIC;
header->size = total_size;
return (void *)((uintptr_t)header + HEADER_SIZE);
```

So the returned address is `page_base + HEADER_SIZE`, which is NOT page-aligned. Any caller requesting page-aligned memory (DMA buffers, page tables, MMIO mappings) will get a misaligned address.

**Fix:** For page-aligned allocations, allocate `size + PAGE_SIZE`, find the next page boundary after the header, and store the header just before it. Or better: use `pmm_alloc_pages` directly for page-aligned allocations.

---

### B5 — Error Paths in `create_user_page_table` Use Wrong Free Function

**File:** `paging.c`, `create_user_page_table`

```c
uintptr_t user_pt = pmm_alloc_page();  // Allocated by PMM
// ...
if (error) {
    kfree((void *)user_pt);  // WRONG — kfree expects a kmalloc header
}
```

`kfree` validates a magic number at `ptr - HEADER_SIZE`. Since the PMM page has no magic number there, this is either:
- An immediate `kernel_panic("kfree: invalid pointer")`, or
- Silent heap corruption if the panic check is missing

**Fix:** Replace `kfree((void *)user_pt)` with `pmm_free_page(user_pt)`.

---

## SERIOUS

### S1 — PMM Has No Locking

**File:** `pmm.c`

`pmm_alloc_page` and `pmm_free_page` perform bitmap read-modify-write without any synchronization. Two concurrent allocations (e.g., two processes forking simultaneously, or an interrupt handler allocating while the kernel allocates) can:
- Both find the same bit clear
- Both set it
- Both return the same physical page — instant memory corruption

**Fix:** Add a PMM spinlock. Disable interrupts while holding it (since timer interrupt → schedule → fork → pmm_alloc is a real call path).

---

### S2 — User Sync Object Syscall Arrays Not Protected

**File:** `syscall.c`, `sys_mutex_create` and friends

```c
static mutex_t user_mutexes[MAX_USER_MUTEXES];
static int mutex_in_use[MAX_USER_MUTEXES] = {0};
```

`sys_mutex_create` iterates `mutex_in_use[]` without any lock. Two processes calling `sys_mutex_create` concurrently can claim the same slot. Same issue exists for `user_condvars[]` and `user_rwlocks[]`.

Additionally, `sys_mutex_destroy` marks `mutex_in_use[i] = 0` without checking if threads are waiting on the mutex. Waiters will sleep on a queue that nobody will ever wake.

**Fix:** Protect these arrays with a kernel spinlock (with interrupts disabled). Check for waiters in destroy calls.

---

### S3 — `sys_waitpid` Busy-Waits

**File:** `syscall.c`, `sys_waitpid`

```c
while (1) {
    // scan children...
    if (!found_child) { break; }
    current->state = PROC_SLEEPING;
    scheduler_yield();
    current->state = PROC_RUNNING;   // Immediately RUNNING again
}
```

This sets `PROC_SLEEPING` then immediately restores `PROC_RUNNING` after yield. The process goes to the back of the ready queue but is never actually blocked. It's a CPU-burning busy wait.

**Fix:** Use `wait_queue_sleep` on a per-process child-exit wait queue. Wake the parent from `process_exit` when a child becomes a zombie.

---

### S4 — `sys_sleep` Busy-Waits With WFI

**File:** `syscall.c`, `sys_sleep`

Enables supervisor interrupts inside the syscall handler, then WFI-loops counting timer ticks. The process stays `PROC_RUNNING` and consumes scheduler cycles every time slice.

**Fix:** Mark the process as `PROC_SLEEPING`, record a wakeup time, and let the timer interrupt handler wake the process when the deadline expires.

---

### S5 — `process_exit` TOCTOU Race on Parent Pointer

**File:** `process.c`, `process_exit`

```c
lock_release(&process_lock);
// WINDOW: parent could exit and be freed here
if (current->parent) {
    signal_send(current->parent, SIGCHLD);  // Use-after-free?
}
```

Between releasing the lock and checking `current->parent`, the parent could exit on another hart (future SMP) or be reaped, freeing the process struct. Even on single-core, a nested interrupt could trigger this.

**Fix:** Send `SIGCHLD` while still holding `process_lock`, or copy the parent pointer under the lock and validate it after.

---

### S6 — `sys_getppid` Always Returns 0

**File:** `syscall.c`, `sys_getppid`

```c
uint64_t sys_getppid(void) {
    // TODO: Implement proper parent tracking
    return 0;
}
```

The process struct has a `parent` pointer. This is a one-line fix.

**Fix:** `return current->parent ? current->parent->pid : 0;`

---

### S7 — Pipe Buffer Operations Not Interrupt-Protected

**File:** `pipe.c`, `pipe_read`, `pipe_write`

Both functions modify `pipe->data_size`, `pipe->read_pos`, and `pipe->write_pos` without disabling interrupts. If a timer interrupt fires between the size check and the actual buffer copy, and schedule switches to the other endpoint of the pipe, both sides could operate on stale positions.

**Fix:** Wrap buffer state modifications in `interrupt_save_disable` / `interrupt_restore` pairs, similar to what mutex.c does.

---

### S8 — `signal_default_action` and `signal_handle` Disagree on SIGUSR1/SIGUSR2

**File:** `signal.c`

`signal_default_action` returns `SIG_IGN` for SIGUSR1/SIGUSR2:
```c
case SIGUSR1:
case SIGUSR2:
    return SIG_IGN;  // Ignore by default
```

But `signal_handle` (the function that actually runs) terminates the process for these signals:
```c
case SIGUSR1:
case SIGUSR2:
    signal_default_term(proc);
    break;
```

POSIX says default action for SIGUSR1/2 is terminate. So `signal_handle` is correct but `signal_default_action` is wrong.

**Fix:** Change `signal_default_action` to return `SIG_DFL` (terminate) for SIGUSR1/SIGUSR2 to match both POSIX and actual behavior.

---

### S9 — Missing TLB Flush in `elf_exec_replace`

**File:** `elf_loader.c`, `elf_exec_replace`

After unmapping old VMAs and mapping new program pages into `proc->page_table`, the function calls `switch_page_table(proc->page_table)` at the end. But this is the **same** page table that was already active — just with modified entries. The TLB may contain stale translations from the old mappings.

**Fix:** Issue `sfence.vma` (via a wrapper) after modifying page table entries and before accessing the new mappings. `switch_page_table` should also issue `sfence.vma` internally if it doesn't already.

---

### S10 — `process_get` Accesses Process Table Without Locking

**File:** `process.c`, `process_get`

Reads `process_table[i]` without holding `process_lock`. Callers like `sys_kill`, `process_setpgid`, etc. use the returned pointer without synchronization — the process could be freed between lookup and use.

**Fix:** Hold `process_lock` during lookup. Consider returning a refcounted handle or performing the operation under the lock.

---

### S11 — `wait_queue_sleep` Silently Fails on kmalloc Failure

**File:** `wait_queue.c`, `wait_queue_sleep`

```c
wait_queue_entry_t *entry = (wait_queue_entry_t *)kmalloc(sizeof(wait_queue_entry_t));
if (!entry) {
    interrupt_restore(old_state);
    return;  // Silent failure — caller has no idea sleep didn't work
}
```

Callers like `mutex_lock` retry in a loop: `while (locked) { wait_queue_sleep(...); }`. If `kmalloc` fails, the process never sleeps, spins checking the lock, calls `wait_queue_sleep` again → infinite busy loop burning 100% CPU.

**Fix:** Either pre-allocate wait queue entries (embed in process struct), or use a static pool. Alternatively, have `wait_queue_sleep` return an error code that callers can handle.

---

### S12 — `enter_user_mode_asm` Window With User SP Before SRET

**File:** `enter_usermode.S`

```asm
mv sp, a0         # sp = user stack
# If a synchronous exception fires HERE → trap handler pushes frame onto user stack
sret
```

Between `mv sp, a0` and `sret`, `sp` points to user memory. SIE is cleared (interrupts disabled), but synchronous exceptions (illegal instruction, page fault, misaligned access) are NOT maskable. If one fires, the trap handler will push a trap frame onto the user stack — corrupting user data and potentially crashing.

**Fix:** Consider using the `sscratch` swap mechanism even for initial entry, or ensure the window is provably exception-free (e.g., verify the two instructions cannot fault).

---

### S13 — `cond_wait` Wakes ALL Mutex Waiters (Thundering Herd)

**File:** `condvar.c`, `cond_wait`

```c
wait_queue_wake(&mutex->waiters);  // Wakes ALL mutex waiters
```

Should wake one. All mutex waiters compete to reacquire, but only one will succeed — the rest spin back to sleep. Wastes CPU and increases contention.

**Fix:** Use `wait_queue_wake_one(&mutex->waiters)`.

---

### S14 — `sys_uname` Reports Stale Version

**File:** `syscall.c`, `sys_uname`

```c
COPY_STR(buf->release, "0.7.0");
COPY_STR(buf->version, "v0.7.0 Virtual Terminals");
```

Project is at v0.9.0 "Synchronization".

**Fix:** Update to `"0.9.0"` and `"v0.9.0 Synchronization"`. Consider generating this from a single `VERSION` define.

---

## MINOR

### M1 — Shell Version String Stale

**File:** `shell.c`

```c
hal_uart_puts("  ThunderOS Shell v0.4.0\n");
```

Should match current version.

---

### M2 — Duplicate `break` in SYS_FORK Case

**File:** `syscall.c`

```c
case SYS_FORK:
    hal_uart_puts("[WARN] SYS_FORK called from old syscall_handler\n");
    return_value = SYSCALL_ERROR;
    break;
    break;  // Dead code
```

---

### M3 — Misleading `(void)` Casts in `scheduler_enqueue`

**File:** `scheduler.c`

```c
(void)queue_count;  // Suppress unused warning - used for debugging
(void)queue_tail;   // Suppress unused warning - used for debugging
```

These variables ARE used on the very next lines. The `(void)` casts are unnecessary and misleading.

---

### M4 — Duplicated Process Initialization Code

**File:** `process.c`

`process_create`, `process_create_user`, and `process_create_elf` all repeat ~20 lines of identical initialization (uid, gid, euid, egid, priority, cwd, signals, controlling_tty, etc.).

**Fix:** Factor into a `process_init_common(struct process *proc, const char *name)` helper.

---

### M5 — Duplicate `kernel_panic` in `user_mode_entry_wrapper`

**File:** `process.c`

Two consecutive panic calls:
```c
kernel_panic("Should never reach here after user_return");
kernel_panic("Should never reach here after enter_user_mode!");
```

The second is dead code.

---

### M6 — `INITIAL_STACK_PAGES` Defined With `#define` Inside Function Body

**File:** `process.c`, `process_create_elf`

```c
int process_create_elf(...) {
    #define INITIAL_STACK_PAGES 8
    // ...
}
```

This `#define` leaks to the rest of the translation unit. Use `static const int` or `enum`, or move to a header.

---

### M7 — `elf_exec_replace` Redefines Constants Already in `constants.h`

**File:** `elf_loader.c`

```c
#define MAX_EXEC_ARGS 16
#define MAX_ARG_LEN 128
```

Both are already defined in `constants.h` (which is included via `kernel/constants.h`). This will cause compiler warnings or silent redefinition.

---

### M8 — `SYS_SETFGPID` Uses `extern` Inside Function Body

**File:** `syscall.c`

```c
case SYS_SETFGPID: {
    extern void vterm_set_active_fg_pid(int pid);
    // ...
}
```

Function-scoped `extern` declarations are fragile and skip type-checking against the actual definition. Include the proper header instead.

---

### M9 — `process_sleep` Ignores `ticks` Parameter

**File:** `process.c`

```c
void process_sleep(uint32_t ticks) {
    (void)ticks;  // TODO: Implement timer-based wakeup
    current->state = PROC_SLEEPING;
}
```

The function marks the process as sleeping but provides no mechanism to wake it after `ticks` timer ticks. Currently dead code since `sys_sleep` implements its own (broken) mechanism.

---

### M10 — Magic Number for Default Process Priority

**File:** `process.c`

```c
proc->priority = 10;  // Default priority
```

Appears in three places. Define `DEFAULT_PROCESS_PRIORITY` in constants.h.

---

## Architecture Notes

A few observations that aren't bugs but will matter as you scale:

1. **kmalloc wastes full pages even for small allocations.** A slab allocator or buddy system would dramatically improve memory utilization. The TODO comment acknowledges this.

2. **The PMM uses O(n) linear scan.** For 128MB of RAM, that's scanning ~32K bits per allocation. Not a problem now, but will be when you add more RAM or heavy allocation patterns. A free list or bitmap tree would help.

3. **The scheduler is a simple circular queue.** This is fine for v0.9, but the priority field on each process is unused by the scheduler — it always picks FIFO. If you want priorities to matter, you'll need multiple queues or a priority-aware selection.

4. **The signal implementation is minimal.** Beyond B3 (signal frame), you'll eventually need: signal masks during handler execution, SA_RESTART semantics, real-time signals, and proper `sigaction` support. The current `signal()` interface is the legacy API.

5. **No support for `vfork` or copy-on-write `fork`.** Every `fork` copies all user pages. This is correct but expensive. COW would be a high-impact optimization.

---

*End of review. Pick a blocker, and we start.*
