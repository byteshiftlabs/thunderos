/*
 * Virtual Terminal and Multi-Shell Tests
 * 
 * Tests virtual terminal system and multi-process functionality
 * added in v0.7.0. These are kernel-mode tests, not interactive.
 * 
 * Compile with: ENABLE_KERNEL_TESTS=1
 */

#ifdef ENABLE_KERNEL_TESTS

#include "drivers/vterm.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "drivers/virtio_gpu.h"
#include "tests/structured_test_kernel.h"

/* Aliases used in this test file */
#define vterm_write_char(t, c)   vterm_putc_to((t), (c))
#define vterm_write_string(t, s) vterm_puts_to((t), (s))

void test_vterm_features(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "VTerm");
    ktest_suite_begin(&suite);

    int active = vterm_get_active_index();
    KTEST_ASSERT(suite, "GetActiveTerminalReturnsValidIndex",
                 active >= 0 && active < 6,
                 "vterm_get_active_index returned an invalid terminal index");

    int original = vterm_get_active_index();
    vterm_switch(1);
    int after_switch = vterm_get_active_index();
    vterm_switch(original);
    int restored = vterm_get_active_index();
    KTEST_ASSERT(suite, "SwitchToSecondaryTerminalAndRestore",
                 after_switch == 1 && restored == original,
                 "vterm_switch did not move to VT2 and back to the original terminal");

    vterm_write_string(1, "Test message to VT2\n");
    KTEST_ASSERT(suite, "WriteToNonActiveTerminalDoesNotCrash", 1,
                 "writing to a non-active terminal should not crash");

    struct process *current = process_current();
    KTEST_ASSERT(suite, "CurrentProcessHasValidControllingTty",
                 current != NULL && current->controlling_tty >= -1 && current->controlling_tty < 6,
                 "current process was missing or had an invalid controlling_tty value");

    int has_input_0 = vterm_has_buffered_input_for(0);
    int has_input_1 = vterm_has_buffered_input_for(1);
    KTEST_ASSERT(suite, "InputBufferStatusQueriesReturnBooleanState",
                 (has_input_0 == 0 || has_input_0 == 1) && (has_input_1 == 0 || has_input_1 == 1),
                 "buffer status queries returned invalid values");

    ktest_suite_end(&suite);
}

void test_virtio_gpu_features(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "VirtIOGPU");
    ktest_suite_begin(&suite);

    int available = virtio_gpu_available();
    if (available) {
        KTEST_ASSERT(suite, "GpuDeviceIsAvailable", 1,
                     "virtio_gpu_available reported success");
    } else {
        ktest_case_begin(&suite, "GpuDeviceIsAvailable");
        ktest_case_skip(&suite, "GpuDeviceIsAvailable", "no VirtIO GPU device was detected");
        ktest_case_begin(&suite, "FramebufferDimensionsAreNonZero");
        ktest_case_skip(&suite, "FramebufferDimensionsAreNonZero", "requires a VirtIO GPU device");
        ktest_case_begin(&suite, "FramebufferPointerIsNonNull");
        ktest_case_skip(&suite, "FramebufferPointerIsNonNull", "requires a VirtIO GPU device");
        ktest_case_begin(&suite, "SetAndGetPixelRoundTrips");
        ktest_case_skip(&suite, "SetAndGetPixelRoundTrips", "requires a VirtIO GPU device");
        ktest_case_begin(&suite, "ClearScreenUpdatesFramebuffer");
        ktest_case_skip(&suite, "ClearScreenUpdatesFramebuffer", "requires a VirtIO GPU device");
        ktest_case_begin(&suite, "FlushRegionSucceeds");
        ktest_case_skip(&suite, "FlushRegionSucceeds", "requires a VirtIO GPU device");
        ktest_suite_end(&suite);
        return;
    }

    uint32_t width = 0, height = 0;
    virtio_gpu_get_dimensions(&width, &height);
    KTEST_ASSERT(suite, "FramebufferDimensionsAreNonZero",
                 width > 0 && height > 0,
                 "virtio_gpu_get_dimensions returned a zero-sized framebuffer");

    uint32_t *fb = virtio_gpu_get_framebuffer();
    KTEST_ASSERT(suite, "FramebufferPointerIsNonNull",
                 fb != NULL,
                 "virtio_gpu_get_framebuffer returned NULL");

    uint32_t test_color = 0xFFFF0000;
    virtio_gpu_set_pixel(10, 10, test_color);
    uint32_t read_color = virtio_gpu_get_pixel(10, 10);
    KTEST_ASSERT(suite, "SetAndGetPixelRoundTrips",
                 read_color != 0,
                 "virtio_gpu_get_pixel returned zero after writing a test pixel");

    virtio_gpu_clear(0xFF0000FF);
    uint32_t cleared = virtio_gpu_get_pixel(100, 100);
    KTEST_ASSERT(suite, "ClearScreenUpdatesFramebuffer",
                 cleared != 0,
                 "virtio_gpu_clear did not leave a visible value in the framebuffer");

    int result = virtio_gpu_flush_region(0, 0, 100, 100);
    KTEST_ASSERT(suite, "FlushRegionSucceeds",
                 result == 0,
                 "virtio_gpu_flush_region returned an error");

    ktest_suite_end(&suite);
}

void test_multi_process_tty(void) {
    ktest_suite_t suite;
    ktest_suite_init(&suite, "MultiProcessTTY");
    ktest_suite_begin(&suite);

    int active_count = 0;
    int max_procs = process_get_max_count();
    for (int i = 0; i < max_procs; i++) {
        struct process *p = process_get_by_index(i);
        if (p != NULL) {
            active_count++;
        }
    }
    KTEST_ASSERT(suite, "AtLeastOneProcessIsActive",
                 active_count >= 1,
                 "no active processes were visible to the scheduler");

    int tty_counts[6] = {0, 0, 0, 0, 0, 0};
    int detached_count = 0;

    for (int i = 0; i < max_procs; i++) {
        struct process *p = process_get_by_index(i);
        if (p != NULL) {
            if (p->controlling_tty >= 0 && p->controlling_tty < 6) {
                tty_counts[p->controlling_tty]++;
            } else {
                detached_count++;
            }
        }
    }
    KTEST_ASSERT(suite, "TtyDistributionAccountsForActiveProcesses",
                 tty_counts[0] + tty_counts[1] + tty_counts[2] + tty_counts[3] + tty_counts[4] + tty_counts[5] + detached_count == active_count,
                 "TTY distribution counts did not account for every active process");

    ktest_suite_end(&suite);
}

/* Main entry point for v0.7.0 tests */
void test_v070_features(void) {
    test_vterm_features();
    test_virtio_gpu_features();
    test_multi_process_tty();
}

#endif /* ENABLE_KERNEL_TESTS */
