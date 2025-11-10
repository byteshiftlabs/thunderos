/*
 * vfs.c - Virtual Filesystem implementation
 */

#include "../../include/fs/vfs.h"
#include "../../include/hal/hal_uart.h"
#include "../../include/mm/kmalloc.h"
#include <stddef.h>

/* Global file descriptor table (per-process would be better, but global for now) */
static vfs_file_t g_file_table[VFS_MAX_OPEN_FILES];

/* Root filesystem */
static vfs_filesystem_t *g_root_fs = NULL;

/**
 * String length
 */
static uint32_t strlen(const char *str) {
    uint32_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * String copy
 */
static void strcpy(char *dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

/**
 * String comparison
 */
static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/**
 * Initialize VFS
 */
int vfs_init(void) {
    /* Initialize file descriptor table */
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        g_file_table[i].node = NULL;
        g_file_table[i].flags = 0;
        g_file_table[i].pos = 0;
        g_file_table[i].in_use = 0;
    }
    
    /* Reserve stdin/stdout/stderr */
    g_file_table[VFS_FD_STDIN].in_use = 1;
    g_file_table[VFS_FD_STDOUT].in_use = 1;
    g_file_table[VFS_FD_STDERR].in_use = 1;
    
    g_root_fs = NULL;
    
    hal_uart_puts("vfs: Initialized\n");
    return 0;
}

/**
 * Mount a filesystem at root
 */
int vfs_mount_root(vfs_filesystem_t *fs) {
    if (!fs || !fs->root) {
        hal_uart_puts("vfs: Invalid filesystem\n");
        return -1;
    }
    
    g_root_fs = fs;
    hal_uart_puts("vfs: Mounted root filesystem (");
    hal_uart_puts(fs->name);
    hal_uart_puts(")\n");
    return 0;
}

/**
 * Allocate a file descriptor
 */
int vfs_alloc_fd(void) {
    for (int i = 3; i < VFS_MAX_OPEN_FILES; i++) {  /* Skip stdin/stdout/stderr */
        if (!g_file_table[i].in_use) {
            g_file_table[i].in_use = 1;
            g_file_table[i].node = NULL;
            g_file_table[i].pos = 0;
            g_file_table[i].flags = 0;
            return i;
        }
    }
    return -1;  /* No free descriptors */
}

/**
 * Free a file descriptor
 */
void vfs_free_fd(int fd) {
    if (fd >= 0 && fd < VFS_MAX_OPEN_FILES) {
        g_file_table[fd].in_use = 0;
        g_file_table[fd].node = NULL;
        g_file_table[fd].pos = 0;
        g_file_table[fd].flags = 0;
    }
}

/**
 * Get file structure from descriptor
 */
vfs_file_t *vfs_get_file(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !g_file_table[fd].in_use) {
        return NULL;
    }
    return &g_file_table[fd];
}

/**
 * Resolve a path to a VFS node
 * Currently only supports absolute paths from root
 */
vfs_node_t *vfs_resolve_path(const char *path) {
    if (!g_root_fs) {
        hal_uart_puts("vfs: No root filesystem mounted\n");
        return NULL;
    }
    
    if (!path || path[0] != '/') {
        hal_uart_puts("vfs: Path must be absolute (start with /)\n");
        return NULL;
    }
    
    /* Root directory */
    if (path[1] == '\0') {
        return g_root_fs->root;
    }
    
    /* Skip leading slash */
    path++;
    
    /* Start from root */
    vfs_node_t *current = g_root_fs->root;
    char component[256];
    uint32_t comp_idx = 0;
    
    while (*path) {
        /* Extract path component */
        comp_idx = 0;
        while (*path && *path != '/') {
            if (comp_idx < 255) {
                component[comp_idx++] = *path;
            }
            path++;
        }
        component[comp_idx] = '\0';
        
        if (comp_idx == 0) {
            /* Skip empty components (e.g., "//") */
            if (*path == '/') {
                path++;
            }
            continue;
        }
        
        /* Lookup component in current directory */
        if (!current->ops || !current->ops->lookup) {
            hal_uart_puts("vfs: No lookup operation\n");
            return NULL;
        }
        
        vfs_node_t *next = current->ops->lookup(current, component);
        if (!next) {
            return NULL;  /* Component not found */
        }
        
        current = next;
        
        /* Skip separator */
        if (*path == '/') {
            path++;
        }
    }
    
    return current;
}

/**
 * Open a file
 */
int vfs_open(const char *path, uint32_t flags) {
    if (!path) {
        hal_uart_puts("vfs: NULL path\n");
        return -1;
    }
    
    /* Resolve path */
    vfs_node_t *node = vfs_resolve_path(path);
    
    /* If file doesn't exist and O_CREAT is set, create it */
    if (!node && (flags & O_CREAT)) {
        /* Extract parent directory and filename */
        /* For now, only support files in root directory */
        if (path[0] == '/' && path[1] != '\0') {
            const char *filename = path + 1;
            
            /* Check if filename contains '/' */
            const char *p = filename;
            while (*p && *p != '/') p++;
            if (*p == '/') {
                hal_uart_puts("vfs: O_CREAT only supports root directory for now\n");
                return -1;
            }
            
            /* Create file in root directory */
            vfs_node_t *root = g_root_fs->root;
            if (root->ops && root->ops->create) {
                int ret = root->ops->create(root, filename, 0644);
                if (ret != 0) {
                    hal_uart_puts("vfs: Failed to create file\n");
                    return -1;
                }
                
                /* Try to resolve again */
                node = vfs_resolve_path(path);
            }
        }
    }
    
    if (!node) {
        hal_uart_puts("vfs: File not found: ");
        hal_uart_puts(path);
        hal_uart_puts("\n");
        return -1;
    }
    
    /* Allocate file descriptor */
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        hal_uart_puts("vfs: No free file descriptors\n");
        return -1;
    }
    
    /* Initialize file descriptor */
    g_file_table[fd].node = node;
    g_file_table[fd].flags = flags;
    g_file_table[fd].pos = 0;
    
    /* Call filesystem open if available */
    if (node->ops && node->ops->open) {
        int ret = node->ops->open(node, flags);
        if (ret != 0) {
            vfs_free_fd(fd);
            return -1;
        }
    }
    
    /* If O_TRUNC, truncate file to zero */
    if (flags & O_TRUNC) {
        node->size = 0;
    }
    
    /* If O_APPEND, seek to end */
    if (flags & O_APPEND) {
        g_file_table[fd].pos = node->size;
    }
    
    return fd;
}

/**
 * Close a file
 */
int vfs_close(int fd) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file) {
        return -1;
    }
    
    /* Call filesystem close if available */
    if (file->node && file->node->ops && file->node->ops->close) {
        file->node->ops->close(file->node);
    }
    
    /* Free the file descriptor */
    vfs_free_fd(fd);
    return 0;
}

/**
 * Read from a file
 */
int vfs_read(int fd, void *buffer, uint32_t size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file || !file->node) {
        return -1;
    }
    
    /* Check if opened for reading */
    if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for reading\n");
        return -1;
    }
    
    /* Check if read operation exists */
    if (!file->node->ops || !file->node->ops->read) {
        hal_uart_puts("vfs: No read operation\n");
        return -1;
    }
    
    /* Read from current position */
    int bytes_read = file->node->ops->read(file->node, file->pos, buffer, size);
    if (bytes_read > 0) {
        file->pos += bytes_read;
    }
    
    return bytes_read;
}

/**
 * Write to a file
 */
int vfs_write(int fd, const void *buffer, uint32_t size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file || !file->node) {
        return -1;
    }
    
    /* Check if opened for writing */
    if ((file->flags & O_RDONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for writing\n");
        return -1;
    }
    
    /* Check if write operation exists */
    if (!file->node->ops || !file->node->ops->write) {
        hal_uart_puts("vfs: No write operation\n");
        return -1;
    }
    
    /* Write at current position */
    int bytes_written = file->node->ops->write(file->node, file->pos, buffer, size);
    if (bytes_written > 0) {
        file->pos += bytes_written;
        
        /* Update file size if we wrote past end */
        if (file->pos > file->node->size) {
            file->node->size = file->pos;
        }
    }
    
    return bytes_written;
}

/**
 * Seek within a file
 */
int vfs_seek(int fd, int offset, int whence) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file || !file->node) {
        return -1;
    }
    
    uint32_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
            
        case SEEK_CUR:
            new_pos = file->pos + offset;
            break;
            
        case SEEK_END:
            new_pos = file->node->size + offset;
            break;
            
        default:
            hal_uart_puts("vfs: Invalid whence value\n");
            return -1;
    }
    
    file->pos = new_pos;
    return new_pos;
}

/**
 * Create a directory
 */
int vfs_mkdir(const char *path, uint32_t mode) {
    if (!g_root_fs || !path) {
        return -1;
    }
    
    /* For now, only support directories in root */
    if (path[0] != '/' || path[1] == '\0') {
        return -1;
    }
    
    const char *dirname = path + 1;
    
    /* Check if name contains '/' */
    const char *p = dirname;
    while (*p && *p != '/') p++;
    if (*p == '/') {
        hal_uart_puts("vfs: mkdir only supports root directory for now\n");
        return -1;
    }
    
    vfs_node_t *root = g_root_fs->root;
    if (!root->ops || !root->ops->mkdir) {
        hal_uart_puts("vfs: No mkdir operation\n");
        return -1;
    }
    
    return root->ops->mkdir(root, dirname, mode);
}

/**
 * Remove a directory
 */
int vfs_rmdir(const char *path) {
    if (!g_root_fs || !path) {
        return -1;
    }
    
    /* For now, only support directories in root */
    if (path[0] != '/' || path[1] == '\0') {
        return -1;
    }
    
    const char *dirname = path + 1;
    
    vfs_node_t *root = g_root_fs->root;
    if (!root->ops || !root->ops->rmdir) {
        hal_uart_puts("vfs: No rmdir operation\n");
        return -1;
    }
    
    return root->ops->rmdir(root, dirname);
}

/**
 * Remove a file
 */
int vfs_unlink(const char *path) {
    if (!g_root_fs || !path) {
        return -1;
    }
    
    /* For now, only support files in root */
    if (path[0] != '/' || path[1] == '\0') {
        return -1;
    }
    
    const char *filename = path + 1;
    
    vfs_node_t *root = g_root_fs->root;
    if (!root->ops || !root->ops->unlink) {
        hal_uart_puts("vfs: No unlink operation\n");
        return -1;
    }
    
    return root->ops->unlink(root, filename);
}

/**
 * Get file status
 */
int vfs_stat(const char *path, uint32_t *size, uint32_t *type) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        return -1;
    }
    
    if (size) {
        *size = node->size;
    }
    if (type) {
        *type = node->type;
    }
    
    return 0;
}

/**
 * Check if file exists
 */
int vfs_exists(const char *path) {
    vfs_node_t *node = vfs_resolve_path(path);
    return node != NULL;
}
