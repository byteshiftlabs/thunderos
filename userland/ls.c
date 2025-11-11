/*
 * ls - List directory contents
 * Simple implementation using getdents syscall
 */

// ThunderOS syscall numbers
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_OPEN 13
#define SYS_CLOSE 14

// Note: ThunderOS doesn't have openat or getdents yet
// For now, ls will just print a message
#define AT_FDCWD -100
#define O_RDONLY 0
#define O_DIRECTORY 0x10000

typedef unsigned long size_t;
typedef long ssize_t;

// System call wrapper
static inline long syscall(long n, long a0, long a1, long a2) {
    register long syscall_num asm("a7") = n;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    
    asm volatile("ecall"
                 : "+r"(arg0)
                 : "r"(syscall_num), "r"(arg1), "r"(arg2)
                 : "memory");
    
    return arg0;
}

// Helper functions
static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void print(const char *s) {
    syscall(SYS_WRITE, 1, (long)s, strlen(s));
}

void _start(void) {
    // For now, just print a test message
    // TODO: Implement proper directory listing when getdents is available
    print("ls: Directory listing:\n");
    print("  test.txt\n");
    print("  bin/\n");
    print("Note: Full directory listing requires getdents syscall (not yet implemented)\n");
    
    syscall(SYS_EXIT, 0, 0, 0);
}
