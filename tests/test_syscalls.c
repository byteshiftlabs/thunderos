/*
 * test_syscalls.c - Test filesystem syscalls
 */

#include "hal/hal_uart.h"
#include "kernel/syscall.h"
#include "fs/vfs.h"
#include <stdint.h>

/* Helper to print decimal number */
static void print_dec(uint32_t value) {
    char buffer[12];
    int i = 0;
    
    if (value == 0) {
        hal_uart_putc('0');
        return;
    }
    
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    while (i > 0) {
        hal_uart_putc(buffer[--i]);
    }
}

void test_syscalls_all(void) {
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("   Filesystem Syscall Tests\n");
    hal_uart_puts("========================================\n\n");
    
    hal_uart_puts("[NOTE] Using existing VFS mount from previous tests\n\n");
    
    int passed = 0;
    int failed = 0;
    
    /* Test 1: sys_open and sys_close */
    hal_uart_puts("[TEST] sys_open and sys_close\n");
    int fd = sys_open("/test.txt", O_RDONLY, 0);
    if (fd >= 0) {
        hal_uart_puts("  [PASS] Opened /test.txt\n");
        passed++;
        
        uint64_t result = sys_close(fd);
        if (result == 0) {
            hal_uart_puts("  [PASS] Closed file\n");
            passed++;
        } else {
            hal_uart_puts("  [FAIL] Failed to close file\n");
            failed++;
        }
    } else {
        hal_uart_puts("  [FAIL] Failed to open /test.txt\n");
        failed += 2;
    }
    
    /* Test 2: sys_read */
    hal_uart_puts("\n[TEST] sys_read\n");
    fd = sys_open("/test.txt", O_RDONLY, 0);
    if (fd >= 0) {
        char buffer[64];
        int bytes_read = sys_read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            hal_uart_puts("  [PASS] Read ");
            print_dec(bytes_read);
            hal_uart_puts(" bytes\n");
            passed++;
        } else {
            hal_uart_puts("  [FAIL] Read failed\n");
            failed++;
        }
        sys_close(fd);
    } else {
        hal_uart_puts("  [FAIL] Failed to open file\n");
        failed++;
    }
    
    /* Test 3: sys_write and sys_read back */
    hal_uart_puts("\n[TEST] sys_write and read back\n");
    fd = sys_open("/test.txt", O_RDWR, 0);
    if (fd >= 0) {
        const char *test_data = "SYSCALL";
        int bytes_written = sys_write(fd, test_data, 7);
        if (bytes_written == 7) {
            hal_uart_puts("  [PASS] Wrote 7 bytes\n");
            passed++;
            
            /* Seek back to start */
            uint64_t new_pos = sys_lseek(fd, 0, SEEK_SET);
            if (new_pos == 0) {
                hal_uart_puts("  [PASS] Seeked to start\n");
                passed++;
                
                char buffer[8] = {0};
                int bytes = sys_read(fd, buffer, 7);
                if (bytes == 7) {
                    /* Simple comparison */
                    int match = 1;
                    for (int i = 0; i < 7; i++) {
                        if (buffer[i] != test_data[i]) {
                            match = 0;
                            break;
                        }
                    }
                    
                    if (match) {
                        hal_uart_puts("  [PASS] Read back data matches\n");
                        passed++;
                    } else {
                        hal_uart_puts("  [FAIL] Data mismatch\n");
                        failed++;
                    }
                } else {
                    hal_uart_puts("  [FAIL] Read failed\n");
                    failed++;
                }
            } else {
                hal_uart_puts("  [FAIL] Seek failed\n");
                failed += 2;
            }
        } else {
            hal_uart_puts("  [FAIL] Write failed\n");
            failed += 3;
        }
        sys_close(fd);
    } else {
        hal_uart_puts("  [FAIL] Failed to open file\n");
        failed += 4;
    }
    
    /* Test 4: sys_stat */
    hal_uart_puts("\n[TEST] sys_stat\n");
    uint32_t stat_buf[2];
    uint64_t result = sys_stat("/test.txt", stat_buf);
    if (result == 0) {
        hal_uart_puts("  [PASS] stat succeeded\n");
        hal_uart_puts("    Size: ");
        print_dec(stat_buf[0]);
        hal_uart_puts(" bytes\n");
        hal_uart_puts("    Type: ");
        print_dec(stat_buf[1]);
        hal_uart_puts("\n");
        passed++;
    } else {
        hal_uart_puts("  [FAIL] stat failed\n");
        failed++;
    }
    
    /* Test 5: sys_mkdir */
    hal_uart_puts("\n[TEST] sys_mkdir\n");
    result = sys_mkdir("/syscalldir", 0755);
    if (result == 0) {
        hal_uart_puts("  [PASS] Created directory\n");
        passed++;
        
        /* Verify it exists */
        result = sys_stat("/syscalldir", stat_buf);
        if (result == 0 && stat_buf[1] == VFS_TYPE_DIRECTORY) {
            hal_uart_puts("  [PASS] Directory exists and is correct type\n");
            passed++;
        } else {
            hal_uart_puts("  [FAIL] Directory not found or wrong type\n");
            failed++;
        }
    } else {
        hal_uart_puts("  [FAIL] mkdir failed\n");
        failed += 2;
    }
    
    /* Test 6: Create file with O_CREAT */
    hal_uart_puts("\n[TEST] sys_open with O_CREAT\n");
    fd = sys_open("/newfile.txt", O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
        hal_uart_puts("  [PASS] Created new file\n");
        passed++;
        
        const char *data = "Hello";
        int bytes = sys_write(fd, data, 5);
        if (bytes == 5) {
            hal_uart_puts("  [PASS] Wrote to new file\n");
            passed++;
        } else {
            hal_uart_puts("  [FAIL] Write to new file failed\n");
            failed++;
        }
        sys_close(fd);
    } else {
        hal_uart_puts("  [FAIL] Failed to create file\n");
        failed += 2;
    }
    
    hal_uart_puts("\n========================================\n");
    hal_uart_puts("Tests passed: ");
    print_dec(passed);
    hal_uart_puts(", Tests failed: ");
    print_dec(failed);
    hal_uart_puts("\n");
    
    if (failed == 0) {
        hal_uart_puts("*** ALL TESTS PASSED ***\n");
    }
    hal_uart_puts("========================================\n\n");
}
