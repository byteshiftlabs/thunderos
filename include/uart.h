/*
 * UART driver header
 */

#ifndef UART_H
#define UART_H

// Initialize UART
void uart_init(void);

// Write a single character
void uart_putc(char c);

// Write a null-terminated string
void uart_puts(const char *s);

// Read a single character (blocking)
char uart_getc(void);

#endif // UART_H
