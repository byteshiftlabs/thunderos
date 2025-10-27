/* 
 * ThunderOS - Boot entry point
 * RISC-V 64-bit assembly
 */

.section .text.boot
.global _start

_start:
    # Disable interrupts
    csrw sie, zero
    
    # Setup stack pointer
    # We'll use 0x80200000 as the base, stack grows downward
    la sp, _stack_top
    
    # Clear BSS section
    la t0, _bss_start
    la t1, _bss_end
clear_bss:
    beq t0, t1, clear_bss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j clear_bss
clear_bss_done:
    
    # Jump to kernel main
    call kernel_main
    
    # If kernel_main returns, halt
halt:
    wfi
    j halt

.section .bss
.align 12
_stack_bottom:
    .space 4096 * 4  # 16KB stack
_stack_top:
