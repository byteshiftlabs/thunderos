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
#include "kernel/signal.h"
#include "kernel/constants.h"
#include "kernel/kstring.h"
#include "hal/hal_uart.h"
#include "mm/paging.h"
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

static int setup_test_user_process(struct process *proc, const char *name, pid_t pid) {
    kmemset(proc, 0, sizeof(*proc));
    proc->pid = pid;
    proc->state = PROC_RUNNING;
    kstrcpy(proc->name, name);

    proc->page_table = create_user_page_table();
    if (!proc->page_table) {
        return -1;
    }

    if (process_setup_memory_isolation(proc) != 0) {
        free_page_table(proc->page_table);
        proc->page_table = NULL;
        return -1;
    }

    return 0;
}

static void cleanup_test_user_process(struct process *proc) {
    if (!proc) {
        return;
    }

    process_cleanup_vmas(proc);
    if (proc->page_table) {
        free_page_table(proc->page_table);
        proc->page_table = NULL;
    }
}

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

static void test_getppid_returns_parent_pid(void) {
    TEST_START("sys_getppid returns parent pid and clears errno");

    struct process *saved_process = process_current();
    struct process parent;
    struct process child;

    kmemset(&parent, 0, sizeof(parent));
    kmemset(&child, 0, sizeof(child));
    parent.pid = 41;
    child.pid = 42;
    child.parent = &parent;

    process_set_current(&child);
    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_getppid() == 41, "sys_getppid should return the parent pid");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared on success");

    process_set_current(saved_process);

    TEST_PASS();
}

static void test_getppid_returns_zero_for_init(void) {
    TEST_START("sys_getppid returns zero for init-like process");

    struct process *saved_process = process_current();
    struct process init_like;

    kmemset(&init_like, 0, sizeof(init_like));
    init_like.pid = 1;
    init_like.parent = NULL;

    process_set_current(&init_like);
    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(sys_getppid() == 0, "sys_getppid should return zero when there is no parent");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK, "errno should be cleared for the no-parent case");

    process_set_current(saved_process);

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

static void test_signal_syscall_dispatch_installs_handler(void) {
    TEST_START("SYS_SIGNAL installs handler through syscall dispatch");

    struct process *current = process_current();
    current->signal_handlers[SIGUSR1] = SIG_DFL;

    clear_errno();
    ASSERT_TRUE(syscall_handler(SYS_SIGNAL, SIGUSR1, (uint64_t)SIG_IGN, 0, 0, 0, 0) == (uint64_t)SIG_DFL,
                "SYS_SIGNAL should return the previous handler");
    ASSERT_TRUE(current->signal_handlers[SIGUSR1] == SIG_IGN,
                "SYS_SIGNAL should install the new handler");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK,
                "SYS_SIGNAL should clear errno on success");

    TEST_PASS();
}

static void test_signal_syscall_invalid_handler_sets_efault(void) {
    TEST_START("SYS_SIGNAL reports EFAULT for invalid handler pointer");

    clear_errno();
    ASSERT_TRUE(syscall_handler(SYS_SIGNAL, SIGUSR1, USER_CODE_BASE, 0, 0, 0, 0) == (uint64_t)-1,
                "SYS_SIGNAL should fail for an unmapped user handler pointer");
    ASSERT_TRUE(get_errno() == THUNDEROS_EFAULT,
                "SYS_SIGNAL should set EFAULT for an invalid handler pointer");

    TEST_PASS();
}

static void test_mutex_destroy_locked_sets_ebusy(void) {
    TEST_START("sys_mutex_destroy reports EBUSY for locked mutex");

    uint64_t mutex_id = sys_mutex_create();
    ASSERT_TRUE(mutex_id != (uint64_t)-1, "sys_mutex_create should succeed");
    ASSERT_TRUE(sys_mutex_lock((int)mutex_id) == 0, "sys_mutex_lock should succeed");

    clear_errno();
    ASSERT_TRUE(sys_mutex_destroy((int)mutex_id) == (uint64_t)-1,
                "sys_mutex_destroy should fail while mutex is locked");
    ASSERT_TRUE(get_errno() == THUNDEROS_EBUSY,
                "sys_mutex_destroy should set EBUSY for a live mutex");

    ASSERT_TRUE(sys_mutex_unlock((int)mutex_id) == 0, "sys_mutex_unlock should succeed after busy destroy");
    ASSERT_TRUE(sys_mutex_destroy((int)mutex_id) == 0, "sys_mutex_destroy should succeed once unlocked");

    TEST_PASS();
}

static void test_mutex_unlock_non_owner_sets_eperm(void) {
    TEST_START("sys_mutex_unlock reports EPERM for non-owner");

    struct process *saved_process = process_current();
    struct process other_process;
    kmemset(&other_process, 0, sizeof(other_process));
    other_process.pid = 99;

    uint64_t mutex_id = sys_mutex_create();
    ASSERT_TRUE(mutex_id != (uint64_t)-1, "sys_mutex_create should succeed");
    ASSERT_TRUE(sys_mutex_lock((int)mutex_id) == 0, "sys_mutex_lock should succeed");

    process_set_current(&other_process);
    clear_errno();
    ASSERT_TRUE(sys_mutex_unlock((int)mutex_id) == (uint64_t)-1,
                "sys_mutex_unlock should fail for a non-owner");
    ASSERT_TRUE(get_errno() == THUNDEROS_EPERM,
                "sys_mutex_unlock should set EPERM for a non-owner");

    process_set_current(saved_process);
    ASSERT_TRUE(sys_mutex_unlock((int)mutex_id) == 0, "owner should still be able to unlock");
    ASSERT_TRUE(sys_mutex_destroy((int)mutex_id) == 0, "sys_mutex_destroy should succeed after unlock");

    TEST_PASS();
}

static void test_rwlock_destroy_live_sets_ebusy(void) {
    TEST_START("sys_rwlock_destroy reports EBUSY for held rwlock");

    uint64_t rwlock_id = sys_rwlock_create();
    ASSERT_TRUE(rwlock_id != (uint64_t)-1, "sys_rwlock_create should succeed");
    ASSERT_TRUE(sys_rwlock_read_lock((int)rwlock_id) == 0, "sys_rwlock_read_lock should succeed");

    clear_errno();
    ASSERT_TRUE(sys_rwlock_destroy((int)rwlock_id) == (uint64_t)-1,
                "sys_rwlock_destroy should fail while readers hold the lock");
    ASSERT_TRUE(get_errno() == THUNDEROS_EBUSY,
                "sys_rwlock_destroy should set EBUSY for a live rwlock");

    ASSERT_TRUE(sys_rwlock_read_unlock((int)rwlock_id) == 0, "sys_rwlock_read_unlock should succeed");
    ASSERT_TRUE(sys_rwlock_destroy((int)rwlock_id) == 0, "sys_rwlock_destroy should succeed after release");

    TEST_PASS();
}

static void test_process_set_tty_invalid_sets_einval(void) {
    TEST_START("process_set_tty reports EINVAL for invalid tty index");

    struct process proc;
    kmemset(&proc, 0, sizeof(proc));

    clear_errno();
    ASSERT_TRUE(process_set_tty(&proc, 99) == -1,
                "process_set_tty should fail for an invalid tty index");
    ASSERT_TRUE(get_errno() == THUNDEROS_EINVAL,
                "process_set_tty should set EINVAL for an invalid tty index");

    TEST_PASS();
}

static void test_process_get_tty_null_sets_einval(void) {
    TEST_START("process_get_tty reports EINVAL for NULL process");

    clear_errno();
    ASSERT_TRUE(process_get_tty(NULL) == -1,
                "process_get_tty should fail for a NULL process");
    ASSERT_TRUE(get_errno() == THUNDEROS_EINVAL,
                "process_get_tty should set EINVAL for a NULL process");

    TEST_PASS();
}

static void test_process_get_tty_clears_errno_on_success(void) {
    TEST_START("process_get_tty clears stale errno on success");

    struct process proc;
    kmemset(&proc, 0, sizeof(proc));
    proc.controlling_tty = -1;

    set_errno(THUNDEROS_EIO);
    ASSERT_TRUE(process_get_tty(&proc) == -1,
                "process_get_tty should return the stored tty value");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK,
                "process_get_tty should clear errno on success even for tty -1");

    TEST_PASS();
}

static void test_mmap_chooses_first_fitting_gap(void) {
    TEST_START("sys_mmap chooses the first fitting free gap");

    struct process *saved_process = process_current();
    struct process proc;

    ASSERT_TRUE(setup_test_user_process(&proc, "mmap_gap", 201) == 0,
                "Test process setup failed");

    ASSERT_TRUE(process_add_vma(&proc, USER_MMAP_START, USER_MMAP_START + PAGE_SIZE, VM_READ | VM_WRITE | VM_USER) == 0,
                "Failed to add first VMA");
    ASSERT_TRUE(process_add_vma(&proc, USER_MMAP_START + (3 * PAGE_SIZE), USER_MMAP_START + (4 * PAGE_SIZE), VM_READ | VM_WRITE | VM_USER) == 0,
                "Failed to add second VMA");

    process_set_current(&proc);
    clear_errno();
    ASSERT_TRUE(sys_mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE, 0, -1, 0) == USER_MMAP_START + PAGE_SIZE,
                "sys_mmap should choose the first gap large enough for the whole mapping");
    ASSERT_TRUE(get_errno() == THUNDEROS_OK,
                "sys_mmap should clear errno on success");

    process_set_current(saved_process);
    cleanup_test_user_process(&proc);

    TEST_PASS();
}

static void test_munmap_rejects_partial_vma(void) {
    TEST_START("sys_munmap rejects partial VMA unmaps");

    struct process *saved_process = process_current();
    struct process proc;
    uint64_t mapped_addr;

    ASSERT_TRUE(setup_test_user_process(&proc, "munmap_partial", 202) == 0,
                "Test process setup failed");

    process_set_current(&proc);
    mapped_addr = sys_mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE, 0, -1, 0);
    ASSERT_TRUE(mapped_addr != (uint64_t)-1, "sys_mmap should create the backing VMA");

    clear_errno();
    ASSERT_TRUE(sys_munmap((void *)mapped_addr, PAGE_SIZE) == (uint64_t)-1,
                "sys_munmap should reject partial VMA removal");
    ASSERT_TRUE(get_errno() == THUNDEROS_EINVAL,
                "sys_munmap should set EINVAL for unsupported partial unmaps");
    ASSERT_TRUE(process_find_vma(&proc, mapped_addr + PAGE_SIZE) != NULL,
                "Partial munmap failure should leave the original VMA intact");

    ASSERT_TRUE(sys_munmap((void *)mapped_addr, PAGE_SIZE * 2) == 0,
                "sys_munmap should still allow exact VMA removal");

    process_set_current(saved_process);
    cleanup_test_user_process(&proc);

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
    test_getppid_returns_parent_pid();
    test_getppid_returns_zero_for_init();
    test_sbrk_zero_clears_errno();
    test_sleep_zero_clears_errno();
    test_yield_clears_errno();
    test_gettime_clears_errno();
    test_signal_syscall_dispatch_installs_handler();
    test_signal_syscall_invalid_handler_sets_efault();
    test_mutex_destroy_locked_sets_ebusy();
    test_mutex_unlock_non_owner_sets_eperm();
    test_rwlock_destroy_live_sets_ebusy();
    test_process_set_tty_invalid_sets_einval();
    test_process_get_tty_null_sets_einval();
    test_process_get_tty_clears_errno_on_success();
    test_mmap_chooses_first_fitting_gap();
    test_munmap_rejects_partial_vma();

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
