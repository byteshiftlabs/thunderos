/*
 * ELF Loader Tests
 * 
 * This file is only compiled when ENABLE_KERNEL_TESTS is defined.
 */

#ifdef ENABLE_KERNEL_TESTS

#include "kernel/elf_loader.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/constants.h"
#include "fs/vfs.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "tests/structured_test_kernel.h"
#include <stdint.h>

static const char *argv_test_args[] = {
    "argv_test",
    "alpha",
    "beta",
    NULL
};

static int read_user_u64(struct process *proc, uint64_t vaddr, uint64_t *value) {
    uintptr_t paddr;
    if (!proc || !value || virt_to_phys(proc->page_table, vaddr, &paddr) != 0) {
        return -1;
    }

    *value = *(uint64_t *)translate_phys_to_virt(paddr);
    return 0;
}

static int read_user_string(struct process *proc, uint64_t vaddr, char *buf, size_t buf_size) {
    if (!proc || !buf || buf_size == 0) {
        return -1;
    }

    for (size_t i = 0; i < buf_size; i++) {
        uintptr_t paddr;
        if (virt_to_phys(proc->page_table, vaddr + i, &paddr) != 0) {
            return -1;
        }

        buf[i] = *(char *)translate_phys_to_virt(paddr);
        if (buf[i] == '\0') {
            return 0;
        }
    }

    return -1;
}

void test_elf_all(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "ELFLoader");
    ktest_suite_begin(&suite);

    int result = elf_load_exec("/nonexistent", NULL, 0);
    KTEST_ASSERT(suite, "RejectsNonexistentFile",
                 result < 0,
                 "elf_load_exec should fail for a nonexistent file");
    
    result = elf_load_exec("/test.txt", NULL, 0);
    KTEST_ASSERT(suite, "RejectsInvalidElfFile",
                 result < 0,
                 "elf_load_exec should fail for a non-ELF file");

    uintptr_t code_page = pmm_alloc_page();
    if (!code_page) {
        KTEST_ASSERT(suite, "FreshElfProcessGetsArgvStack", 0,
                     "could not allocate a page for the synthetic ELF test");
    } else {
        struct process *proc = process_create_elf("argv_test", USER_CODE_BASE, (void *)code_page,
                                                  PAGE_SIZE, USER_CODE_BASE, NULL, 0,
                                                  argv_test_args, 3);
        if (!proc) {
            pmm_free_page(code_page);
            KTEST_ASSERT(suite, "FreshElfProcessGetsArgvStack", 0,
                         "could not create a synthetic ELF process");
        } else {
            uint64_t argv_base = proc->trap_frame->a1;
            uint64_t arg0 = 0;
            uint64_t arg1 = 0;
            char arg0_buf[16];
            char arg1_buf[16];

            int ok = proc->trap_frame->a0 == 3 &&
                     argv_base != 0 &&
                     read_user_u64(proc, argv_base, &arg0) == 0 &&
                     read_user_u64(proc, argv_base + sizeof(uint64_t), &arg1) == 0 &&
                     read_user_string(proc, arg0, arg0_buf, sizeof(arg0_buf)) == 0 &&
                     read_user_string(proc, arg1, arg1_buf, sizeof(arg1_buf)) == 0;

            KTEST_ASSERT(suite, "FreshElfProcessGetsArgvStack",
                         ok &&
                         arg0_buf[0] == 'a' && arg0_buf[1] == 'r' &&
                         arg1_buf[0] == 'a' && arg1_buf[1] == 'l',
                         "argc/argv were not copied to the initial user stack correctly");

            scheduler_dequeue(proc);
            process_free(proc);
        }
    }

    ktest_suite_end(&suite);
}

#endif // ENABLE_KERNEL_TESTS
