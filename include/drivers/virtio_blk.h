/**
 * VirtIO Block Device Driver
 * 
 * Implementation of VirtIO block device specification for disk I/O.
 * Supports both legacy (v1) and modern (v2) VirtIO devices.
 * 
 * Reference: VirtIO Specification 1.1
 */

#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#include <stdint.h>
#include <stddef.h>

/* VirtIO MMIO Register Offsets (from base address) */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000  // Magic value ('virt')
#define VIRTIO_MMIO_VERSION             0x004  // Device version
#define VIRTIO_MMIO_DEVICE_ID           0x008  // Device type (2 = block)
#define VIRTIO_MMIO_VENDOR_ID           0x00c  // Vendor ID
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010  // Device features
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014  // Device features selector
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020  // Driver features
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024  // Driver features selector
#define VIRTIO_MMIO_QUEUE_SEL           0x030  // Queue selector
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034  // Maximum queue size
#define VIRTIO_MMIO_QUEUE_NUM           0x038  // Queue size
#define VIRTIO_MMIO_QUEUE_READY         0x044  // Queue ready
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050  // Queue notify
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060  // Interrupt status
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064  // Interrupt acknowledge
#define VIRTIO_MMIO_STATUS              0x070  // Device status
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080  // Queue descriptor address (low)
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084  // Queue descriptor address (high)
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090  // Available ring address (low)
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094  // Available ring address (high)
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0  // Used ring address (low)
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4  // Used ring address (high)
#define VIRTIO_MMIO_CONFIG_GENERATION   0x0fc  // Configuration generation
#define VIRTIO_MMIO_CONFIG              0x100  // Device-specific configuration

/* VirtIO Magic Value */
#define VIRTIO_MAGIC                    0x74726976  // 'virt' in little-endian

/* VirtIO Device IDs */
#define VIRTIO_DEVICE_ID_BLOCK          2

/* VirtIO Status Bits */
#define VIRTIO_STATUS_ACKNOWLEDGE       (1 << 0)  // Guest OS has noticed device
#define VIRTIO_STATUS_DRIVER            (1 << 1)  // Guest OS knows how to drive device
#define VIRTIO_STATUS_DRIVER_OK         (1 << 2)  // Driver is ready
#define VIRTIO_STATUS_FEATURES_OK       (1 << 3)  // Features negotiated successfully
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET (1 << 6) // Device experienced error
#define VIRTIO_STATUS_FAILED            (1 << 7)  // Fatal error occurred

/* VirtIO Block Device Features */
#define VIRTIO_BLK_F_SIZE_MAX           (1 << 1)  // Maximum segment size
#define VIRTIO_BLK_F_SEG_MAX            (1 << 2)  // Maximum number of segments
#define VIRTIO_BLK_F_GEOMETRY           (1 << 4)  // Legacy geometry
#define VIRTIO_BLK_F_RO                 (1 << 5)  // Read-only device
#define VIRTIO_BLK_F_BLK_SIZE           (1 << 6)  // Block size
#define VIRTIO_BLK_F_FLUSH              (1 << 9)  // Cache flush command
#define VIRTIO_BLK_F_TOPOLOGY           (1 << 10) // Topology information
#define VIRTIO_BLK_F_CONFIG_WCE         (1 << 11) // Write cache enable

/* VirtIO Block Request Types */
#define VIRTIO_BLK_T_IN                 0         // Read
#define VIRTIO_BLK_T_OUT                1         // Write
#define VIRTIO_BLK_T_FLUSH              4         // Flush
#define VIRTIO_BLK_T_DISCARD            11        // Discard
#define VIRTIO_BLK_T_WRITE_ZEROES       13        // Write zeros

/* VirtIO Block Request Status */
#define VIRTIO_BLK_S_OK                 0         // Success
#define VIRTIO_BLK_S_IOERR              1         // I/O error
#define VIRTIO_BLK_S_UNSUPP             2         // Unsupported operation

/* VirtIO Descriptor Flags */
#define VIRTQ_DESC_F_NEXT               1         // This descriptor continues
#define VIRTQ_DESC_F_WRITE              2         // Write-only (device writes)
#define VIRTQ_DESC_F_INDIRECT           4         // Indirect descriptor

/* VirtIO Used Ring Flags */
#define VIRTQ_USED_F_NO_NOTIFY          1         // Don't notify when buffer added

/* VirtIO Available Ring Flags */
#define VIRTQ_AVAIL_F_NO_INTERRUPT      1         // Don't interrupt when buffer used

/* Block device sector size */
#define VIRTIO_BLK_SECTOR_SIZE          512

/* Default queue size (must be power of 2) */
#define VIRTIO_BLK_QUEUE_SIZE           128

/**
 * VirtIO Block Device Configuration Space
 * Located at offset 0x100 from MMIO base
 */
typedef struct {
    uint64_t capacity;          // Device capacity in 512-byte sectors
    uint32_t size_max;          // Maximum segment size
    uint32_t seg_max;           // Maximum number of segments
    struct {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;          // Block size (power of 2, >= 512)
    struct {
        uint8_t physical_block_exp;  // Exponent for physical block size
        uint8_t alignment_offset;    // Alignment offset
        uint16_t min_io_size;        // Minimum I/O size
        uint32_t opt_io_size;        // Optimal I/O size
    } topology;
    uint8_t writeback;          // Write cache enabled
    uint8_t unused0[3];
    uint32_t max_discard_sectors;    // Maximum discard sectors
    uint32_t max_discard_seg;        // Maximum discard segments
    uint32_t discard_sector_alignment;  // Discard sector alignment
    uint32_t max_write_zeroes_sectors;  // Maximum write zeroes sectors
    uint32_t max_write_zeroes_seg;      // Maximum write zeroes segments
    uint8_t write_zeroes_may_unmap;     // Write zeroes may unmap
    uint8_t unused1[3];
} __attribute__((packed)) virtio_blk_config_t;

/**
 * VirtQueue Descriptor
 * Describes a single buffer in the virtqueue
 */
typedef struct {
    uint64_t addr;              // Physical address
    uint32_t len;               // Length
    uint16_t flags;             // Flags (VIRTQ_DESC_F_*)
    uint16_t next;              // Next descriptor index (if NEXT flag set)
} __attribute__((packed)) virtq_desc_t;

/**
 * VirtQueue Available Ring
 * Written by driver, read by device
 */
typedef struct {
    uint16_t flags;             // Flags (VIRTQ_AVAIL_F_*)
    uint16_t idx;               // Index of next available descriptor
    uint16_t ring[];            // Available descriptor indices (size = queue_size)
    // Note: 'used_event' follows ring[], at ring[queue_size]
} __attribute__((packed)) virtq_avail_t;

/**
 * VirtQueue Used Element
 * Single element in the used ring
 */
typedef struct {
    uint32_t id;                // Descriptor chain head index
    uint32_t len;               // Total bytes written to buffer
} __attribute__((packed)) virtq_used_elem_t;

/**
 * VirtQueue Used Ring
 * Written by device, read by driver
 */
typedef struct {
    uint16_t flags;             // Flags (VIRTQ_USED_F_*)
    uint16_t idx;               // Index of next used descriptor
    virtq_used_elem_t ring[];   // Used descriptor elements (size = queue_size)
    // Note: 'avail_event' follows ring[], at ring[queue_size]
} __attribute__((packed)) virtq_used_t;

/**
 * VirtQueue
 * Complete virtqueue structure with descriptor, available, and used rings
 */
typedef struct {
    uint32_t queue_size;        // Number of descriptors
    uint16_t last_seen_used;    // Last used index we've seen
    
    // DMA-allocated rings
    virtq_desc_t *desc;         // Descriptor ring
    virtq_avail_t *avail;       // Available ring
    virtq_used_t *used;         // Used ring
    
    // Physical addresses for device
    uintptr_t desc_phys;
    uintptr_t avail_phys;
    uintptr_t used_phys;
    
    // Free descriptor tracking
    uint16_t free_head;         // Head of free descriptor list
    uint16_t num_free;          // Number of free descriptors
} virtqueue_t;

/**
 * VirtIO Block Request Header
 * Sent to device for each I/O operation
 */
typedef struct {
    uint32_t type;              // Request type (VIRTIO_BLK_T_*)
    uint32_t reserved;          // Reserved (must be zero)
    uint64_t sector;            // First sector to read/write
} __attribute__((packed)) virtio_blk_req_header_t;

/**
 * VirtIO Block Request
 * Complete request structure including header, data buffer, and status
 */
typedef struct {
    virtio_blk_req_header_t header;  // Request header
    uint8_t *data;                   // Data buffer (DMA-allocated)
    uint8_t status;                  // Status byte (written by device)
} virtio_blk_request_t;

/**
 * VirtIO Block Device
 * Main driver state structure
 */
typedef struct {
    uintptr_t base_addr;        // MMIO base address
    uint32_t irq;               // Interrupt number
    
    // Device information
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t version;
    uint64_t features;          // Negotiated features
    
    // Block device properties
    uint64_t capacity;          // Capacity in sectors
    uint32_t block_size;        // Block size in bytes
    uint8_t read_only;          // Read-only flag
    
    // VirtQueue
    virtqueue_t queue;
    
    // Statistics
    uint64_t read_count;
    uint64_t write_count;
    uint64_t error_count;
} virtio_blk_device_t;

/* Function Prototypes */

/**
 * Initialize VirtIO block device driver
 * @param base_addr MMIO base address of the device
 * @param irq Interrupt number
 * @return 0 on success, negative on error
 */
int virtio_blk_init(uintptr_t base_addr, uint32_t irq);

/**
 * Read sectors from block device
 * @param sector Starting sector number
 * @param buffer Buffer to read into (must be DMA-capable)
 * @param count Number of sectors to read
 * @return Number of sectors read, negative on error
 */
int virtio_blk_read(uint64_t sector, void *buffer, uint32_t count);

/**
 * Write sectors to block device
 * @param sector Starting sector number
 * @param buffer Buffer to write from (must be DMA-capable)
 * @param count Number of sectors to write
 * @return Number of sectors written, negative on error
 */
int virtio_blk_write(uint64_t sector, const void *buffer, uint32_t count);

/**
 * Flush device write cache
 * @return 0 on success, negative on error
 */
int virtio_blk_flush(void);

/**
 * Get device capacity in sectors
 * @return Capacity in 512-byte sectors
 */
uint64_t virtio_blk_get_capacity(void);

/**
 * Get device block size
 * @return Block size in bytes
 */
uint32_t virtio_blk_get_block_size(void);

/**
 * Check if device is read-only
 * @return 1 if read-only, 0 if writable
 */
int virtio_blk_is_readonly(void);

/**
 * VirtIO block device interrupt handler
 */
void virtio_blk_irq_handler(void);

/**
 * Get the global VirtIO block device
 * @return Pointer to device structure, or NULL if not initialized
 */
virtio_blk_device_t *virtio_blk_get_device(void);

#endif /* VIRTIO_BLK_H */
