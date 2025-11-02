/*
 * Kernel Memory Allocator (kmalloc/kfree)
 * 
 * Provides dynamic memory allocation for the kernel.
 * Simple implementation using the PMM as a backend.
 */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

/**
 * Allocate kernel memory
 * 
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *kmalloc(size_t size);

/**
 * Free previously allocated kernel memory
 * 
 * @param ptr Pointer to memory to free (from kmalloc)
 */
void kfree(void *ptr);

/**
 * Allocate aligned kernel memory
 * 
 * @param size Number of bytes to allocate
 * @param align Alignment requirement (must be power of 2)
 * @return Pointer to aligned allocated memory, or NULL if out of memory
 */
void *kmalloc_aligned(size_t size, size_t align);

#endif // KMALLOC_H
