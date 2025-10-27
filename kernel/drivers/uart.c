/*
 * UART driver for RISC-V (NS16550A compatible)
 * Used in QEMU virt machine
 */

#include "uart.h"

// UART0 base address on QEMU virt machine
#define UART0_BASE 0x10000000

// UART registers
#define UART_RBR (UART0_BASE + 0)  // Receiver Buffer Register
#define UART_THR (UART0_BASE + 0)  // Transmitter Holding Register
#define UART_LSR (UART0_BASE + 5)  // Line Status Register

#define LSR_TX_IDLE (1 << 5)       // Transmitter idle

// Helper to write to UART register
static inline void uart_write_reg(unsigned long addr, unsigned char val) {
    *(volatile unsigned char *)addr = val;
}

// Helper to read from UART register
static inline unsigned char uart_read_reg(unsigned long addr) {
    return *(volatile unsigned char *)addr;
}

void uart_init(void) {
    // QEMU's UART is already initialized by OpenSBI
    // No additional setup needed for basic operation
}

void uart_putc(char c) {
    // Wait until transmitter is idle
    while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
        ;
    
    uart_write_reg(UART_THR, c);
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

char uart_getc(void) {
    // Wait for data to be available
    while ((uart_read_reg(UART_LSR) & 1) == 0)
        ;
    
    return uart_read_reg(UART_RBR);
}
