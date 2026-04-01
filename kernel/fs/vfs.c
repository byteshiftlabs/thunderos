/*
 * vfs.c - Virtual Filesystem implementation
 */

#include "../../include/fs/vfs.h"
#include "../../include/fs/ext2.h"
#include "../../include/hal/hal_uart.h"
#include "../../include/mm/kmalloc.h"
#include "../../include/kernel/errno.h"
#include "../../include/kernel/pipe.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/constants.h"
#include <stddef.h>

/* ========================================================================
 * Constants
 * ======================================================================== */

/* Use MAX_PATH_COMPONENTS and MAX_PATH_COMPONENT_LEN from constants.h */

/* ========================================================================
 * Global state
 * ======================================================================== */

/* Boot-time fallback descriptor table used before a current process exists. */
static vfs_file_t g_boot_file_table[VFS_MAX_OPEN_FILES];

/* Root filesystem */
static vfs_filesystem_t *g_root_fs = NULL;

/* ========================================================================
 * Forward declarations
 * ======================================================================== */

static int normalize_build_absolute(const char *path, char *working, size_t working_size);
static int normalize_resolve_components(const char *working, char *normalized, size_t size);
static int vfs_is_persistent_node(vfs_node_t *node);
static vfs_file_t *vfs_current_file_table(void);
static void vfs_reset_fd_entry(vfs_file_t *file_table, int fd);
static void vfs_retain_file(vfs_file_t *file);
static int vfs_close_in_table(vfs_file_t *file_table, int fd);

/* ========================================================================
 * Path normalization helpers
 * ======================================================================== */

/**
 * normalize_build_absolute - Build absolute path from relative path and cwd
 * 
 * @param path Input path (relative or absolute)
 * @param working Output buffer for absolute path
 * @param working_size Size of output buffer
 * @return 0 on success, -1 on error (errno set)
 */
static int normalize_build_absolute(const char *path, char *working, size_t working_size) {
    size_t pos = 0;
    
    if (path[0] != '/') {
        /* Relative path - prepend current working directory */
        struct process *current_process = process_current();
        if (current_process && current_process->cwd[0]) {
            const char *cwd = current_process->cwd;
            while (*cwd) {
                if (pos >= working_size - 1) {
                    set_errno(THUNDEROS_ERANGE);
                    return -1;
                }
                working[pos++] = *cwd++;
            }
        } else {
            /* Default to root if no cwd set */
            if (pos >= working_size - 1) {
                set_errno(THUNDEROS_ERANGE);
                return -1;
            }
            working[pos++] = '/';
        }
        
        /* Ensure trailing slash for concatenation */
        if (pos > 0 && working[pos - 1] != '/') {
            if (pos >= working_size - 1) {
                set_errno(THUNDEROS_ERANGE);
                return -1;
            }
            working[pos++] = '/';
        }
    }
    
    /* Append the input path */
    while (*path) {
        if (pos >= working_size - 1) {
            set_errno(THUNDEROS_ERANGE);
            return -1;
        }
        working[pos++] = *path++;
    }
    working[pos] = '\0';
    
    return 0;
}

/**
 * normalize_resolve_components - Resolve . and .. in path components
 * 
 * Uses a stack-based approach: push regular components, pop for ..
 * 
 * @param working Input absolute path
 * @param normalized Output buffer for resolved path
 * @param size Size of output buffer
 * @return 0 on success, -1 on error (errno set)
 */
static int normalize_resolve_components(const char *working, char *normalized, size_t size) {
    /* Component storage - each component is a null-terminated string */
    char component_storage[MAX_PATH_COMPONENTS][MAX_PATH_COMPONENT_LEN];
    int component_count = 0;
    
    const char *cursor = working;
    if (*cursor == '/') {
        cursor++;  /* Skip leading slash */
    }
    
    while (*cursor) {
        /* Find component boundaries */
        const char *component_start = cursor;
        while (*cursor && *cursor != '/') {
            cursor++;
        }
        
        size_t component_length = cursor - component_start;
        
        if (component_length == 0 || (component_length == 1 && component_start[0] == '.')) {
            /* Empty component (double slash) or current directory ".", skip */
        } else if (component_length == 2 && component_start[0] == '.' && component_start[1] == '.') {
            /* Parent directory "..", pop if possible */
            if (component_count > 0) {
                component_count--;
            }
            /* At root, silently ignore (can't go above root) */
        } else {
            /* Regular component, copy to storage */
            if (component_count >= MAX_PATH_COMPONENTS || component_length >= MAX_PATH_COMPONENT_LEN) {
                set_errno(THUNDEROS_ERANGE);
                return -1;
            }

            for (size_t i = 0; i < component_length; i++) {
                component_storage[component_count][i] = component_start[i];
            }
            component_storage[component_count][component_length] = '\0';
            component_count++;
        }
        
        if (*cursor == '/') {
            cursor++;
        }
    }
    
    /* Build the normalized path from components */
    size_t output_pos = 0;
    if (size < 2) {
        set_errno(THUNDEROS_ERANGE);
        return -1;
    }
    normalized[output_pos++] = '/';
    
    for (int component_index = 0; component_index < component_count; component_index++) {
        const char *component = component_storage[component_index];
        while (*component) {
            if (output_pos >= size - 1) {
                set_errno(THUNDEROS_ERANGE);
                return -1;
            }
            normalized[output_pos++] = *component++;
        }
        
        /* Add separator between components (not after last) */
        if (component_index < component_count - 1) {
            if (output_pos >= size - 1) {
                set_errno(THUNDEROS_ERANGE);
                return -1;
            }
            normalized[output_pos++] = '/';
        }
    }
    normalized[output_pos] = '\0';
    
    clear_errno();
    return 0;
}

static int vfs_is_persistent_node(vfs_node_t *node) {
    return node && g_root_fs && node == g_root_fs->root;
}

static vfs_file_t *vfs_current_file_table(void) {
    struct process *proc = process_current();
    if (proc) {
        return proc->fd_table;
    }
    return g_boot_file_table;
}

static void vfs_reset_fd_entry(vfs_file_t *file_table, int fd) {
    file_table[fd].in_use = 0;
    file_table[fd].node = NULL;
    file_table[fd].pos = 0;
    file_table[fd].flags = 0;
    file_table[fd].pipe = NULL;
    file_table[fd].type = VFS_TYPE_FILE;
}

static void vfs_retain_node(vfs_node_t *node) {
    if (!node || vfs_is_persistent_node(node)) {
        return;
    }

    node->ref_count++;
}

static void vfs_retain_pipe(pipe_t *pipe, uint32_t flags) {
    if (!pipe) {
        return;
    }

    if ((flags & O_RDWR) == O_RDWR) {
        pipe->read_ref_count++;
        pipe->write_ref_count++;
    } else if (flags & O_WRONLY) {
        pipe->write_ref_count++;
    } else {
        pipe->read_ref_count++;
    }
}

static void vfs_retain_file(vfs_file_t *file) {
    if (!file || !file->in_use) {
        return;
    }

    if (file->type == VFS_TYPE_PIPE) {
        vfs_retain_pipe((pipe_t *)file->pipe, file->flags);
        return;
    }

    vfs_retain_node(file->node);
}

static int vfs_close_in_table(vfs_file_t *file_table, int fd) {
    vfs_file_t *file = &file_table[fd];

    if (!file->in_use) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }

    if (file->type == VFS_TYPE_PIPE && file->pipe) {
        pipe_t *pipe = (pipe_t *)file->pipe;

        if ((file->flags & O_RDWR) == O_RDWR) {
            pipe_close_read(pipe);
            pipe_close_write(pipe);
        } else if (file->flags & O_WRONLY) {
            pipe_close_write(pipe);
        } else {
            pipe_close_read(pipe);
        }

        if (pipe_can_free(pipe)) {
            pipe_free(pipe);
        }
    }

    if (file->node) {
        vfs_release_node(file->node);
    }

    vfs_reset_fd_entry(file_table, fd);
    clear_errno();
    return 0;
}

void vfs_release_node(vfs_node_t *node) {
    if (!node || vfs_is_persistent_node(node)) {
        return;
    }

    if (node->ref_count > 1) {
        node->ref_count--;
        return;
    }

    if (node->ops && node->ops->close) {
        node->ops->close(node);
    }

    if (node->fs_data) {
        kfree(node->fs_data);
    }

    kfree(node);
}

/* ========================================================================
 * Public path resolution API
 * ======================================================================== */

/**
 * vfs_normalize_path - Convert relative path to absolute, resolve . and ..
 * 
 * @param path Input path (relative or absolute)
 * @param normalized Output buffer for normalized absolute path
 * @param size Size of output buffer
 * @return 0 on success, -1 on error
 * 
 * @errno THUNDEROS_EINVAL - Invalid parameters
 */
int vfs_normalize_path(const char *path, char *normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        set_errno(THUNDEROS_EINVAL);
        return -1;
    }
    
    /* Working buffer for intermediate absolute path */
    char working_buffer[VFS_MAX_PATH];
    
    /* Step 1: Build absolute path from relative + cwd */
    if (normalize_build_absolute(path, working_buffer, VFS_MAX_PATH) < 0) {
        return -1;
    }
    
    /* Step 2: Resolve . and .. components */
    if (normalize_resolve_components(working_buffer, normalized, size) < 0) {
        return -1;
    }
    
    clear_errno();
    return 0;
}

/**
 * Initialize VFS
 */
int vfs_init(void) {
    vfs_init_file_table(g_boot_file_table);
    
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
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    g_root_fs = fs;
    hal_uart_puts("vfs: Mounted root filesystem (");
    hal_uart_puts(fs->name);
    hal_uart_puts(")\n");
    clear_errno();
    return 0;
}

/**
 * Allocate a file descriptor
 */
int vfs_alloc_fd(void) {
    vfs_file_t *file_table = vfs_current_file_table();

    for (int i = VFS_FD_FIRST_REGULAR; i < VFS_MAX_OPEN_FILES; i++) {
        if (!file_table[i].in_use) {
            file_table[i].in_use = 1;
            file_table[i].node = NULL;
            file_table[i].pos = 0;
            file_table[i].flags = 0;
            file_table[i].pipe = NULL;
            file_table[i].type = VFS_TYPE_FILE;
            clear_errno();
            return i;
        }
    }
    /* No free descriptors */
    RETURN_ERRNO(THUNDEROS_EMFILE);
}

/**
 * Free a file descriptor
 */
void vfs_free_fd(int fd) {
    vfs_file_t *file_table = vfs_current_file_table();

    if (fd >= 0 && fd < VFS_MAX_OPEN_FILES) {
        vfs_reset_fd_entry(file_table, fd);
    }
}

/**
 * Duplicate a file descriptor
 * 
 * Makes newfd be the copy of oldfd, closing newfd first if necessary.
 * 
 * @param oldfd The file descriptor to duplicate
 * @param newfd The target file descriptor number
 * @return newfd on success, -1 on error
 */
int vfs_dup2(int oldfd, int newfd) {
    vfs_file_t *file_table = vfs_current_file_table();

    /* Validate newfd range */
    if (newfd < 0 || newfd >= VFS_MAX_OPEN_FILES) {
        set_errno(THUNDEROS_EINVAL);
        return -1;
    }
    
    /* Get the source file */
    if (oldfd < 0 || oldfd >= VFS_MAX_OPEN_FILES || !file_table[oldfd].in_use) {
        set_errno(THUNDEROS_EBADF);
        return -1;
    }
    
    /* If oldfd == newfd, just return newfd */
    if (oldfd == newfd) {
        clear_errno();
        return newfd;
    }
    
    vfs_file_t *old_file = &file_table[oldfd];
    vfs_file_t *new_file = &file_table[newfd];
    
    /* Close newfd if it's open */
    if (new_file->in_use) {
        vfs_close(newfd);
    }
    
    /* Copy the file descriptor */
    new_file->node = old_file->node;
    new_file->flags = old_file->flags;
    new_file->pos = old_file->pos;
    new_file->in_use = 1;
    new_file->pipe = old_file->pipe;
    new_file->type = old_file->type;
    vfs_retain_file(new_file);
    
    clear_errno();
    return newfd;
}

/**
 * Get file structure from descriptor
 */
vfs_file_t *vfs_get_file(int fd) {
    vfs_file_t *file_table = vfs_current_file_table();

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        set_errno(THUNDEROS_EBADF);
        return NULL;
    }
    return &file_table[fd];
}

void vfs_init_file_table(vfs_file_t *file_table) {
    if (!file_table) {
        return;
    }

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        file_table[i].node = NULL;
        file_table[i].flags = 0;
        file_table[i].pos = 0;
        file_table[i].in_use = 0;
        file_table[i].pipe = NULL;
        file_table[i].type = VFS_TYPE_FILE;
    }

    file_table[VFS_FD_STDIN].in_use = 1;
    file_table[VFS_FD_STDOUT].in_use = 1;
    file_table[VFS_FD_STDERR].in_use = 1;
}

int vfs_clone_file_table(vfs_file_t *dst, vfs_file_t *src) {
    if (!dst || !src) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    vfs_init_file_table(dst);

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!src[i].in_use) {
            continue;
        }

        dst[i] = src[i];
        vfs_retain_file(&dst[i]);
    }

    clear_errno();
    return 0;
}

void vfs_release_file_table(vfs_file_t *file_table) {
    if (!file_table) {
        return;
    }

    for (int i = VFS_FD_FIRST_REGULAR; i < VFS_MAX_OPEN_FILES; i++) {
        if (file_table[i].in_use) {
            (void)vfs_close_in_table(file_table, i);
        }
    }

    vfs_init_file_table(file_table);
}
/**
 * vfs_resolve_path - Resolve a path to a VFS node
 * 
 * Supports both absolute and relative paths. Relative paths are resolved
 * against the current process's working directory.
 * 
 * @param path Path to resolve (absolute or relative)
 * @return VFS node on success, NULL on error (errno set)
 * 
 * @errno THUNDEROS_EFS_NOTMNT - No root filesystem mounted
 * @errno THUNDEROS_EINVAL - Invalid path
 * @errno THUNDEROS_ENOENT - Path component not found
 */
vfs_node_t *vfs_resolve_path(const char *path) {
    if (!g_root_fs) {
        set_errno(THUNDEROS_EFS_NOTMNT);
        return NULL;
    }
    
    if (!path) {
        set_errno(THUNDEROS_EINVAL);
        return NULL;
    }
    
    /* Normalize path (handles relative paths, ., ..) */
    char normalized_path[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized_path, VFS_MAX_PATH) < 0) {
        /* errno already set by vfs_normalize_path */
        return NULL;
    }
    
    /* Root directory special case */
    if (normalized_path[0] == '/' && normalized_path[1] == '\0') {
        return g_root_fs->root;
    }
    
    /* Skip leading slash and walk the path */
    const char *cursor = normalized_path + 1;
    vfs_node_t *current_node = g_root_fs->root;
    char component_name[MAX_PATH_COMPONENT_LEN];
    
    while (*cursor) {
        /* Extract path component */
        uint32_t name_index = 0;
        while (*cursor && *cursor != '/') {
            if (name_index < MAX_PATH_COMPONENT_LEN - 1) {
                component_name[name_index++] = *cursor;
            }
            cursor++;
        }
        component_name[name_index] = '\0';
        
        if (name_index == 0) {
            /* Skip empty components (shouldn't happen after normalize) */
            if (*cursor == '/') {
                cursor++;
            }
            continue;
        }
        
        /* Lookup component in current directory */
        if (!current_node->ops || !current_node->ops->lookup) {
            set_errno(THUNDEROS_EIO);
            if (!vfs_is_persistent_node(current_node)) {
                vfs_release_node(current_node);
            }
            return NULL;
        }
        
        vfs_node_t *next_node = current_node->ops->lookup(current_node, component_name);
        if (!next_node) {
            /* errno already set by lookup */
            if (!vfs_is_persistent_node(current_node)) {
                vfs_release_node(current_node);
            }
            return NULL;
        }

        if (!vfs_is_persistent_node(current_node)) {
            vfs_release_node(current_node);
        }
        
        current_node = next_node;
        
        /* Skip separator */
        if (*cursor == '/') {
            cursor++;
        }
    }
    
    return current_node;
}

/**
 * Open a file
 */
int vfs_open(const char *path, uint32_t flags) {
    vfs_file_t *file_table = vfs_current_file_table();

    if (!path) {
        hal_uart_puts("vfs: NULL path\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Resolve path */
    vfs_node_t *node = vfs_resolve_path(normalized);
    
    /* If file doesn't exist and O_CREAT is set, create it */
    if (!node && (flags & O_CREAT)) {
        /* Extract parent directory and filename */
        /* For now, only support files in root directory */
        if (normalized[0] == '/' && normalized[1] != '\0') {
            const char *filename = normalized + 1;
            
            /* Check if filename contains '/' */
            const char *p = filename;
            while (*p && *p != '/') p++;
            if (*p == '/') {
                hal_uart_puts("vfs: O_CREAT only supports root directory for now\n");
                RETURN_ERRNO(THUNDEROS_EINVAL);
            }
            
            /* Create file in root directory */
            vfs_node_t *root = g_root_fs->root;
            if (root->ops && root->ops->create) {
                int ret = root->ops->create(root, filename, VFS_DEFAULT_FILE_MODE);
                if (ret != 0) {
                    hal_uart_puts("vfs: Failed to create file\n");
                    /* errno already set by create */
                    return -1;
                }
                
                /* Try to resolve again */
                node = vfs_resolve_path(normalized);
            }
        }
    }
    
    if (!node) {
        hal_uart_puts("vfs: File not found: ");
        hal_uart_puts(path);
        hal_uart_puts("\n");
        if (errno == 0) {
            RETURN_ERRNO(THUNDEROS_ENOENT);
        }
        return -1;
    }
    
    /* Check permissions based on open flags */
    int access_mode = 0;
    if ((flags & O_RDWR) == O_RDWR) {
        access_mode = VFS_ACCESS_READ | VFS_ACCESS_WRITE;
    } else if (flags & O_WRONLY) {
        access_mode = VFS_ACCESS_WRITE;
    } else {
        access_mode = VFS_ACCESS_READ;  /* O_RDONLY is 0 */
    }
    
    if (vfs_check_permission(node, access_mode) != 0) {
        vfs_release_node(node);
        /* errno already set by vfs_check_permission */
        return -1;
    }
    
    /* Allocate file descriptor */
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        hal_uart_puts("vfs: No free file descriptors\n");
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Initialize file descriptor */
    file_table[fd].node = node;
    file_table[fd].flags = flags;
    file_table[fd].pos = 0;
    
    /* Call filesystem open if available */
    if (node->ops && node->ops->open) {
        int ret = node->ops->open(node, flags);
        if (ret != 0) {
            vfs_free_fd(fd);
            vfs_release_node(node);
            /* errno already set by open */
            return -1;
        }
    }
    
    /* If O_TRUNC, truncate file to zero */
    if (flags & O_TRUNC) {
        node->size = 0;
    }
    
    /* If O_APPEND, seek to end */
    if (flags & O_APPEND) {
        file_table[fd].pos = node->size;
    }
    
    clear_errno();
    return fd;
}

/**
 * Close a file
 */
int vfs_close(int fd) {
    vfs_file_t *file_table = vfs_current_file_table();

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }

    return vfs_close_in_table(file_table, fd);
}

/**
 * Read from a file
 */
int vfs_read(int fd, void *buffer, uint32_t size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (!file) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    /* Handle pipe read */
    if (file->type == VFS_TYPE_PIPE) {
        if (!file->pipe) {
            RETURN_ERRNO(THUNDEROS_EINVAL);
        }
        return pipe_read((pipe_t*)file->pipe, buffer, size);
    }
    
    /* Regular file read */
    if (!file->node) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }
    
    /* Check if opened for reading */
    if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for reading\n");
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Check if read operation exists */
    if (!file->node->ops || !file->node->ops->read) {
        hal_uart_puts("vfs: No read operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
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
    if (!file) {
        /* errno already set by vfs_get_file */
        return -1;
    }
    
    /* Handle pipe write */
    if (file->type == VFS_TYPE_PIPE) {
        if (!file->pipe) {
            RETURN_ERRNO(THUNDEROS_EINVAL);
        }
        return pipe_write((pipe_t*)file->pipe, buffer, size);
    }
    
    /* Regular file write */
    if (!file->node) {
        RETURN_ERRNO(THUNDEROS_EBADF);
    }
    
    /* Check if opened for writing */
    if ((file->flags & O_RDONLY) && !(file->flags & O_RDWR)) {
        hal_uart_puts("vfs: File not open for writing\n");
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Check if write operation exists */
    if (!file->node->ops || !file->node->ops->write) {
        hal_uart_puts("vfs: No write operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
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
        /* errno already set by vfs_get_file */
        return -1;
    }

    int64_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
            
        case SEEK_CUR:
            new_pos = (int64_t)file->pos + offset;
            break;
            
        case SEEK_END:
            new_pos = (int64_t)file->node->size + offset;
            break;
            
        default:
            hal_uart_puts("vfs: Invalid whence value\n");
            RETURN_ERRNO(THUNDEROS_EINVAL);
    }

    if (new_pos < 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    file->pos = (uint32_t)new_pos;
    clear_errno();
    return (int)new_pos;
}

/**
 * Create a directory
 */
int vfs_mkdir(const char *path, uint32_t mode) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* For now, only support directories in root */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    const char *dirname = normalized + 1;
    
    /* Check if name contains '/' */
    const char *p = dirname;
    while (*p && *p != '/') p++;
    if (*p == '/') {
        hal_uart_puts("vfs: mkdir only supports root directory for now\n");
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *root = g_root_fs->root;
    if (!root->ops || !root->ops->mkdir) {
        hal_uart_puts("vfs: No mkdir operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    return root->ops->mkdir(root, dirname, mode);
}

/**
 * Remove a directory
 */
int vfs_rmdir(const char *path) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Can't remove root */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Find the last slash to separate parent path and directory name */
    char *last_slash = NULL;
    for (char *p = normalized; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    vfs_node_t *parent_dir;
    const char *dirname;
    
    if (last_slash == normalized) {
        /* Directory is in root (e.g., /emptydir) */
        parent_dir = g_root_fs->root;
        dirname = normalized + 1;
    } else {
        /* Directory is in a subdirectory (e.g., /foo/bar) */
        *last_slash = '\0';  /* Temporarily terminate to get parent path */
        parent_dir = vfs_resolve_path(normalized);
        *last_slash = '/';   /* Restore */
        
        if (!parent_dir) {
            if (errno == 0) {
                RETURN_ERRNO(THUNDEROS_ENOENT);
            }
            return -1;
        }
        dirname = last_slash + 1;
    }
    
    if (!parent_dir->ops || !parent_dir->ops->rmdir) {
        hal_uart_puts("vfs: No rmdir operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Check write permission on parent directory */
    if (vfs_check_permission(parent_dir, VFS_ACCESS_WRITE) != 0) {
        if (!vfs_is_persistent_node(parent_dir)) {
            vfs_release_node(parent_dir);
        }
        /* errno already set by vfs_check_permission */
        return -1;
    }

    int result = parent_dir->ops->rmdir(parent_dir, dirname);
    if (!vfs_is_persistent_node(parent_dir)) {
        vfs_release_node(parent_dir);
    }
    return result;
}

/**
 * Remove a file
 */
int vfs_unlink(const char *path) {
    if (!g_root_fs || !path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Normalize path to absolute */
    char normalized[VFS_MAX_PATH];
    if (vfs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        /* errno already set */
        return -1;
    }
    
    /* Must have a filename */
    if (normalized[0] != '/' || normalized[1] == '\0') {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Find the last slash to separate parent path and filename */
    char *last_slash = NULL;
    for (char *p = normalized; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    vfs_node_t *parent_dir;
    const char *filename;
    
    if (last_slash == normalized) {
        /* File is in root (e.g., /deleteme.txt) */
        parent_dir = g_root_fs->root;
        filename = normalized + 1;
    } else {
        /* File is in a subdirectory (e.g., /foo/bar.txt) */
        *last_slash = '\0';  /* Temporarily terminate to get parent path */
        parent_dir = vfs_resolve_path(normalized);
        *last_slash = '/';   /* Restore */
        
        if (!parent_dir) {
            if (errno == 0) {
                RETURN_ERRNO(THUNDEROS_ENOENT);
            }
            return -1;
        }
        filename = last_slash + 1;
    }
    
    if (!parent_dir->ops || !parent_dir->ops->unlink) {
        hal_uart_puts("vfs: No unlink operation\n");
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Check write permission on parent directory */
    if (vfs_check_permission(parent_dir, VFS_ACCESS_WRITE) != 0) {
        if (!vfs_is_persistent_node(parent_dir)) {
            vfs_release_node(parent_dir);
        }
        /* errno already set by vfs_check_permission */
        return -1;
    }

    int result = parent_dir->ops->unlink(parent_dir, filename);
    if (!vfs_is_persistent_node(parent_dir)) {
        vfs_release_node(parent_dir);
    }
    return result;
}

/**
 * Get file status
 */
int vfs_stat(const char *path, uint32_t *size, uint32_t *type) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return -1;
    }
    
    if (size) {
        *size = node->size;
    }
    if (type) {
        *type = node->type;
    }
    
    vfs_release_node(node);
    clear_errno();
    return 0;
}

/**
 * Get extended file status including permissions
 * 
 * @param path      File path
 * @param statbuf   Buffer to fill with stat information
 * @return 0 on success, -1 on error (errno set)
 */
int vfs_stat_full(const char *path, vfs_stat_t *statbuf) {
    if (!path || !statbuf) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return -1;
    }
    
    statbuf->st_ino = node->inode;
    statbuf->st_mode = node->mode;
    statbuf->st_uid = node->uid;
    statbuf->st_gid = node->gid;
    statbuf->st_size = node->size;
    statbuf->st_type = node->type;
    
    vfs_release_node(node);
    clear_errno();
    return 0;
}

/**
 * Check if file exists
 */
int vfs_exists(const char *path) {
    vfs_node_t *node = vfs_resolve_path(path);
    int exists = (node != NULL);
    if (node) {
        vfs_release_node(node);
    }
    return exists;
}

/* ========================================================================
 * Permission Checking
 * ======================================================================== */

/**
 * Check if current process has permission to access a file
 * 
 * Uses standard Unix permission model:
 * - If process euid is 0 (root), always allow access
 * - If process euid matches file uid, use owner permissions
 * - If process egid matches file gid, use group permissions  
 * - Otherwise, use other permissions
 * 
 * @param node     VFS node to check
 * @param mode     Access mode (VFS_ACCESS_READ, VFS_ACCESS_WRITE, VFS_ACCESS_EXEC)
 * @return 0 if access allowed, -1 if denied (errno set to THUNDEROS_EACCES)
 */
int vfs_check_permission(vfs_node_t *node, int mode) {
    if (!node) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    struct process *proc = process_current();
    if (!proc) {
        /* No current process (kernel context), allow access */
        return 0;
    }
    
    uint16_t euid = proc->euid;
    uint16_t egid = proc->egid;
    uint16_t file_mode = node->mode;
    
    /* Root (euid 0) can access anything */
    if (euid == 0) {
        clear_errno();
        return 0;
    }
    
    int allowed = 0;
    
    if (euid == node->uid) {
        /* Check owner permissions */
        if ((mode & VFS_ACCESS_READ) && (file_mode & EXT2_S_IRUSR)) allowed |= VFS_ACCESS_READ;
        if ((mode & VFS_ACCESS_WRITE) && (file_mode & EXT2_S_IWUSR)) allowed |= VFS_ACCESS_WRITE;
        if ((mode & VFS_ACCESS_EXEC) && (file_mode & EXT2_S_IXUSR)) allowed |= VFS_ACCESS_EXEC;
    } else if (egid == node->gid) {
        /* Check group permissions */
        if ((mode & VFS_ACCESS_READ) && (file_mode & EXT2_S_IRGRP)) allowed |= VFS_ACCESS_READ;
        if ((mode & VFS_ACCESS_WRITE) && (file_mode & EXT2_S_IWGRP)) allowed |= VFS_ACCESS_WRITE;
        if ((mode & VFS_ACCESS_EXEC) && (file_mode & EXT2_S_IXGRP)) allowed |= VFS_ACCESS_EXEC;
    } else {
        /* Check other permissions */
        if ((mode & VFS_ACCESS_READ) && (file_mode & EXT2_S_IROTH)) allowed |= VFS_ACCESS_READ;
        if ((mode & VFS_ACCESS_WRITE) && (file_mode & EXT2_S_IWOTH)) allowed |= VFS_ACCESS_WRITE;
        if ((mode & VFS_ACCESS_EXEC) && (file_mode & EXT2_S_IXOTH)) allowed |= VFS_ACCESS_EXEC;
    }
    
    if ((mode & allowed) == mode) {
        clear_errno();
        return 0;
    }
    
    RETURN_ERRNO(THUNDEROS_EACCES);
}

/**
 * Change file permissions (mode bits)
 * 
 * @param path     Path to file
 * @param mode     New permission bits (e.g., 0755)
 * @return 0 on success, -1 on error
 */
int vfs_chmod(const char *path, uint32_t new_mode) {
    if (!path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return -1;
    }
    
    struct process *proc = process_current();
    
    /* Only root or file owner can change permissions */
    if (proc && proc->euid != 0 && proc->euid != node->uid) {
        vfs_release_node(node);
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Update the node's mode (preserve file type bits, update permission bits) */
    node->mode = (node->mode & EXT2_S_IFMT) | (new_mode & 0xFFF);
    
    /* Update the inode on disk if this is ext2 */
    if (node->fs_data) {
        ext2_inode_t *inode = (ext2_inode_t *)node->fs_data;
        inode->i_mode = node->mode;
        
        ext2_fs_t *ext2_fs = (ext2_fs_t *)node->fs->fs_data;
        if (ext2_fs) {
            ext2_write_inode(ext2_fs, node->inode, inode);
        }
    }
    
    vfs_release_node(node);
    clear_errno();
    return 0;
}

/**
 * Change file owner and group
 * 
 * @param path     Path to file
 * @param uid      New owner user ID
 * @param gid      New owner group ID
 * @return 0 on success, -1 on error
 */
int vfs_chown(const char *path, uint16_t uid, uint16_t gid) {
    if (!path) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        /* errno already set by vfs_resolve_path */
        return -1;
    }
    
    struct process *proc = process_current();
    
    /* Only root can change ownership */
    if (proc && proc->euid != 0) {
        vfs_release_node(node);
        RETURN_ERRNO(THUNDEROS_EACCES);
    }
    
    /* Update the node */
    node->uid = uid;
    node->gid = gid;
    
    /* Update the inode on disk if this is ext2 */
    if (node->fs_data) {
        ext2_inode_t *inode = (ext2_inode_t *)node->fs_data;
        inode->i_uid = uid;
        inode->i_gid = gid;
        
        ext2_fs_t *ext2_fs = (ext2_fs_t *)node->fs->fs_data;
        if (ext2_fs) {
            ext2_write_inode(ext2_fs, node->inode, inode);
        }
    }
    
    vfs_release_node(node);
    clear_errno();
    return 0;
}

/* ========================================================================
 * Pipe Support
 * ======================================================================== */

/**
 * Create a pipe and return two file descriptors
 * 
 * Creates an anonymous pipe for inter-process communication.
 * pipefd[0] is the read end, pipefd[1] is the write end.
 * 
 * @param pipefd Array of 2 integers to store file descriptors
 * @return 0 on success, -1 on error
 */
int vfs_create_pipe(int pipefd[2]) {
    vfs_file_t *file_table = vfs_current_file_table();

    if (!pipefd) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Create the pipe */
    pipe_t *pipe = pipe_create();
    if (!pipe) {
        /* errno already set by pipe_create */
        return -1;
    }
    
    /* Allocate read file descriptor */
    int read_fd = vfs_alloc_fd();
    if (read_fd < 0) {
        pipe_free(pipe);
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Allocate write file descriptor */
    int write_fd = vfs_alloc_fd();
    if (write_fd < 0) {
        vfs_free_fd(read_fd);
        pipe_free(pipe);
        /* errno already set by vfs_alloc_fd */
        return -1;
    }
    
    /* Set up read end (pipefd[0]) */
    file_table[read_fd].pipe = pipe;
    file_table[read_fd].type = VFS_TYPE_PIPE;
    file_table[read_fd].flags = O_RDONLY;
    file_table[read_fd].node = NULL;
    file_table[read_fd].pos = 0;
    
    /* Set up write end (pipefd[1]) */
    file_table[write_fd].pipe = pipe;
    file_table[write_fd].type = VFS_TYPE_PIPE;
    file_table[write_fd].flags = O_WRONLY;
    file_table[write_fd].node = NULL;
    file_table[write_fd].pos = 0;
    
    /* Return file descriptors */
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    clear_errno();
    return 0;
}
