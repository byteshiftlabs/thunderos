/*
 * ext2_write.c - ext2 write operations (simplified implementation)
 */

#include "../include/fs/ext2.h"
#include "../include/hal/hal_uart.h"

/**
 * Write data to a file (stub - not yet implemented)
 */
int ext2_write_file(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t offset,
                    const void *buffer, uint32_t size) {
    (void)fs;
    (void)inode;
    (void)offset;
    (void)buffer;
    (void)size;
    
    hal_uart_puts("ext2: write_file not yet implemented\n");
    return -1;
}

/**
 * Create a new file in a directory (stub)
 */
int ext2_create_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    (void)mode;
    
    hal_uart_puts("ext2: create_file not yet implemented\n");
    return -1;
}

/**
 * Create a new directory (stub)
 */
int ext2_create_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name, uint32_t mode) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    (void)mode;
    
    hal_uart_puts("ext2: create_dir not yet implemented\n");
    return -1;
}

/**
 * Remove a file from a directory (stub)
 */
int ext2_remove_file(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    
    hal_uart_puts("ext2: remove_file not yet implemented\n");
    return -1;
}

/**
 * Remove a directory (stub)
 */
int ext2_remove_dir(ext2_fs_t *fs, ext2_inode_t *dir_inode, const char *name) {
    (void)fs;
    (void)dir_inode;
    (void)name;
    
    hal_uart_puts("ext2: remove_dir not yet implemented\n");
    return -1;
}
