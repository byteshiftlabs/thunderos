/*
 * CLINT (Core Local Interruptor) driver for RISC-V
 * Handles timer and software interrupts
 */

#ifndef CLINT_H
#define CLINT_H

// Initialize CLINT and set up timer
void clint_init(void);

// Get current timer tick count
unsigned long clint_get_ticks(void);

// Set next timer interrupt (relative to current time)
void clint_set_timer(unsigned long ticks_from_now);

// Handle timer interrupt
void clint_handle_timer(void);

#endif // CLINT_H
