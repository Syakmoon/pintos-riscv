#include "devices/virtio-blk.h"
#include <ctype.h>
#include <debug.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "devices/partition.h"
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#ifndef MACHINE
#include "threads/palloc.h"
#endif

/* The code in this file is an interface to a Virtual I/O block device.
   Specification is from:
   https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html. */

/* Virtio MMIO addresses. */
#define reg(ADDR) ((volatile uint32_t *) (ADDR))              /* Returns a word pointer. */
#define reg_magic(DEV) (reg((DEV)->reg_base + 0x000))         /* The magic value. */
#define reg_ver(DEV) (reg((DEV)->reg_base + 0x004))           /* Version number. */
#define reg_dev_id(DEV) (reg((DEV)->reg_base + 0x008))        /* Device ID. */
#define reg_vend_id(DEV) (reg((DEV)->reg_base + 0x00c))       /* Vendor ID. */
#define reg_dev_features(DEV) (reg((DEV)->reg_base + 0x010))  /* Device features. */
#define reg_drv_features(DEV) (reg((DEV)->reg_base + 0x020))  /* Driver features. */
#define reg_queue_sel(DEV) (reg((DEV)->reg_base + 0x030))     /* Virtual queue index. */
#define reg_queue_num_max(DEV) (reg((DEV)->reg_base + 0x034)) /* Maximum virtual queue size. */
#define reg_queue_num(DEV) (reg((DEV)->reg_base + 0x038))     /* Virtual queue size. */
#define reg_queue_ready(DEV) (reg((DEV)->reg_base + 0x044))   /* Virtual queue ready. */
#define reg_queue_notify(DEV) (reg((DEV)->reg_base + 0x050))  /* Queue notifier. */
#define reg_intr_status(DEV) (reg((DEV)->reg_base + 0x60))    /* Interrupt status. */
#define reg_intr_ack(DEV) (reg((DEV)->reg_base + 0x064))      /* Interrupt acknowledge. */
#define reg_status(DEV) (reg((DEV)->reg_base + 0x070))        /* Device status. */
#define reg_desc_low(DEV) (reg((DEV)->reg_base + 0x080))      /* Descriptor Area low 32-bit. */
#define reg_desc_high(DEV) (reg((DEV)->reg_base + 0x084))     /* Descriptor Area high 32-bit. */
#define reg_drv_low(DEV) (reg((DEV)->reg_base + 0x090))       /* Driver Area low 32-bit. */
#define reg_drv_high(DEV) (reg((DEV)->reg_base + 0x094))      /* Driver Area high 32-bit. */
#define reg_dev_low(DEV) (reg((DEV)->reg_base + 0x0a0))       /* Device Area low 32-bit. */
#define reg_dev_high(DEV) (reg((DEV)->reg_base + 0x0a4))      /* Device Area high 32-bit. */

/* Status Register bits. */
#define STA_ACK 1     /* ACKNOWLEDGE. */
#define STA_DRV 2     /* DRIVER. */
#define STA_DRV_OK 4  /* DRIVER_OK. */
#define STA_F_OK 8    /* FEATURES_OK. */
#define STA_FAIL 128  /* FAILED. */

/* Feature bits */
#define VIRTIO_F_RO            (1 << 5)	  /* Read-only. */
#define VIRTIO_F_CONFIG_WCE    (1 << 11)	/* We must not do write-back. */
#define VIRTIO_F_EVENT_IDX     (1 << 29)  /* used_event and avail_event. */

/* Configuration space. */
#define reg_conf(DEV) ((DEV)->reg_base + 0x100) /* The address. */
#define conf_cap(DEV) (reg_conf(DEV) + 0x000)   /* Disk capacity. */

/* The size of virtqueue. Must be a power of 2. */
#define QUEUE_SIZE 16

/* Virtio device descriptor.
   From [virtio-v1.2] 2.7.5 "The Virtqueue Descriptor Table". */
struct virtq_desc {
  uint64_t addr;        /* Address (guest-physical). */
  uint32_t len;         /* Length. */

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT   1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE  2

  uint16_t flags;       /* The flags as indicated above. */
  uint16_t next;        /* Next field if flags & NEXT */
};

/* Virtio device available ring.
   From [virtio-v1.2] 2.7.6 "The Virtqueue Available Ring". */
struct virtq_avail {
#define VIRTQ_AVAIL_F_NO_INTERRUPT      1
  uint16_t flags;               /* Flags for the available queue. */
  uint16_t idx;                 /* The next index to use. */
  uint16_t ring[QUEUE_SIZE];
  uint16_t used_event UNUSED;   /* Only if VIRTIO_F_EVENT_IDX */
};
 
/* Virtio device used ring element.
   uint32_t is used here for ids for padding reasons.
   From [virtio-v1.2] 2.7.6 "The Virtqueue Used Ring". */
struct virtq_used_elem {
  uint32_t id;                  /* Index of start of used descriptor chain. */

  /* The number of bytes written into the device writable portion of
     the buffer described by the descriptor chain. */
  uint32_t len;
};

/* Virtio device used ring.
   From [virtio-v1.2] 2.7.8 "The Virtqueue Used Ring". */
struct virtq_used {
#define VIRTQ_USED_F_NO_NOTIFY  1
  uint16_t flags;               /* Flags for the used queue. */
  uint16_t idx;                 /* The next index to use. */
  struct virtq_used_elem ring[QUEUE_SIZE];
  uint16_t avail_event UNUSED;  /* Only if VIRTIO_F_EVENT_IDX */
};

/* Virtio device operation.
   From [virtio-v1.2] 5.2.6 "Device Operation". */
struct virtio_blk_req {
#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
  uint32_t type;
  uint32_t reserved UNUSED;
  uint64_t sector;
};

/* Virtio device response.
   From QEMU virtio-blk.h. */
struct virtio_blk_resp {
#define VIRTIO_BLK_S_OK		0
  uint8_t status;
};

/* A virtio block device. */
struct virtio_blk {
  struct virtq_desc* desc;  /* Descriptor table. */
  struct virtq_avail* avail;/* Avaialbe ring. */
  struct virtq_used* used;  /* Used ring. */

  /* All requests created in memory to avoid using the heap. */
  struct virtio_blk_req req[QUEUE_SIZE];
  struct virtio_blk_resp resp[QUEUE_SIZE];

  uint16_t last_seen_used;  /* The index of the used ring we saw last time. */
  uint16_t next_desc_idx;   /* The next index in the descriptor table to use. */
  uint16_t next_avail_idx;  /* The next index in the avail ring to use. */
  uint16_t in_used;         /* Current number of descriptors in used. */

  char name[8];             /* Name, e.g. "hda". */
  uintptr_t reg_base;       /* Base MMIO address. */
  uint8_t irq;              /* Interrupt in use. */
  enum virtio_blk_mode mode;/* Polling mode or interrupt mode. */

  #ifndef MACHINE
  struct lock lock;                 /* Must acquire to access the device. */
  struct semaphore completion_wait; /* Up'd by interrupt handler. */
  #endif

  bool is_blk;              /* Is device a virtio block device? */
};

/* QEMU's RISC-V emulation supports 8 virtio devices.
   Although x86 Pintos supports only up to 4 hard disks,
   in case we want to add more virtio devices, we can still assume 8 devices. */
#define VIRTIO_CNT 8
static struct virtio_blk blks[VIRTIO_CNT];
#define VIRTIO_MMIO_PHYS_START 0x10001000L

#ifdef MACHINE
/* Set of features we would NOT use in M-mode. */
static const uint32_t excluded_features = VIRTIO_F_CONFIG_WCE |
                                          VIRTIO_F_EVENT_IDX;

#else
/* Set of features we would DEFINITELY NOT use in S-mode, or the kernel. */
static const uint32_t excluded_features = VIRTIO_F_RO |
                                          VIRTIO_F_CONFIG_WCE |
                                          VIRTIO_F_EVENT_IDX;

#endif

static struct block_operations virtio_operations;

static void reset_device(struct virtio_blk*);
static bool check_device_type(struct virtio_blk*);
static void identify_virtio_device(struct virtio_blk*);
static void virtqueue_setup(struct virtio_blk*);

static void write_desc(struct virtq_desc*, uint64_t, uint32_t, uint16_t, uint16_t);
static void recycle_desc(struct virtio_blk*, uint16_t);
static bool receive_pending(struct virtio_blk*);
static bool alloc_while_busy(struct virtio_blk*, size_t);

static void interrupt_handler(struct intr_frame*);

/* Virtqueue size calculation, based on [virtio-v1.2] 2.7 "Split Virtqueues".
   Based on our DMA format, we can ignore alignment, because DESC is at the
   beginning of a page, and AVAIL is always aligned to 2 bytes because of
   DESC.  Used is also at the beginning of a page. */

/* Calculate the size of the descriptor table based on QSZ, the Queue Size. */
static inline unsigned desc_size(unsigned int qsz) { 
  return 16 * qsz; 
}

/* Calculate the size of the available ring based on QSZ, the Queue Size. */
static inline unsigned avail_size(unsigned int qsz) { 
  return 6 + 2 * qsz;
}

/* Calculate the size of the used ring based on QSZ, the Queue Size. */
static inline unsigned used_size(unsigned int qsz) { 
  return 6 + 8 * qsz;
}

static inline bool compare_string_32(uint32_t value, char* expected) {
  return (((char*) &value)[0] == expected[0] &&
      ((char*) &value)[1] == expected[1] &&
      ((char*) &value)[2] == expected[2] &&
      ((char*) &value)[3] == expected[3]);
}

/* Initialize a specific block device.
   If it was successfully initialized, BLK->is_blk will be set to true.
   Otherwise it may silently return or call PANIC. */
void virtio_blk_init(struct virtio_blk* blk, enum virtio_blk_mode mode) {
  uint32_t status, features;

  /* Distinguish hard disks from other devices, or no device. */
  if (!check_device_type(blk))
    return;

  /* The following is the steps to initialize the device from the spec.
  
    1. Reset the device.
    2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    3. Set the DRIVER status bit: the guest OS knows how to drive the device.
    4. Read device feature bits, and write the subset of feature bits
       understood by the OS and driver to the device.  During this step
       the driver MAY read (but MUST NOT write) the device-specific
       configuration fields to check that it can support the device
       before accepting it.
    6. Re-read device status to ensure the FEATURES_OK bit is still set:
       otherwise, the device does not support our subset of features
       and the device is unusable.
    7. Perform device-specific setup, including discovery of virtqueues
       for the device, optional per-bus setup, reading and possibly writing
       the device’s virtio configuration space, and population of virtqueues.
    8. Set the DRIVER_OK status bit. At this point the device is “live”. */
  reset_device(blk);

  status = 0;
  outl(reg_status(blk), status |= STA_ACK);

  outl(reg_status(blk), status |= STA_DRV);

  features = inl(reg_dev_features(blk));
  features &= ~excluded_features;
  outl(reg_drv_features(blk), features);
  outl(reg_status(blk), status |= STA_F_OK);

  status = inl(reg_status(blk));
  if (!(status & STA_F_OK))
    PANIC("Tht set of features for %s are not supported", blk->name);

  /* Device-specific setup for block devices. */
  virtqueue_setup(blk);

  /* We have finished the setup, and are ready to use the device now. */
  outl(reg_status(blk), status |= STA_DRV_OK);

  /* Register interrupt handler. */
  intr_register_ext(blk->irq, interrupt_handler, blk->name);
}

/* Initialize the disk subsystem and detect disks. */
void virtio_blks_init(enum virtio_blk_mode mode) {
  size_t dev_no;

  ASSERT(sizeof(struct virtio_blk_resp) == 1);

  for (dev_no = 0; dev_no < VIRTIO_CNT; dev_no++) {
    struct virtio_blk* blk = &blks[dev_no];

    /* Initialize the metadata for the device. */
    memset(blk, 0, sizeof(struct virtio_blk));
    snprintf(blk->name, sizeof blk->name, "hd%c", 'a' + dev_no);
    blk->reg_base = VIRTIO_MMIO_PHYS_START + 0x1000 * dev_no;
    blk->reg_base = pagedir_set_mmio(init_page_dir, blk->reg_base, 0x1000, true);
    blk->irq = 0x1 + dev_no;
    #ifndef MACHINE
    lock_init(&blk->lock);
    sema_init(&blk->completion_wait, 0);
    #endif
    blk->mode = mode;
    blk->is_blk = false;

    /* Initializes this device. */
    virtio_blk_init(blk, mode);

    /* Read hard disk identity information. */
    if (blk->is_blk)
      identify_virtio_device(blk);
  }
}

/* Disk detection and identification. */

static void descramble_virtio_string(uint32_t, char* dest);

/* Resets a virtio block device. */
static inline void reset_device(struct virtio_blk* blk) {
  outl(reg_status(blk), 0);
  __sync_synchronize();
}

/* Checks whether device D is an ATA disk and sets D's is_blk
   member appropriately. */
static bool check_device_type(struct virtio_blk* d) {
  if (!compare_string_32(inl(reg_magic(d)), "virt"))
    goto not_block_device;
  if (inl(reg_ver(d)) != 0x2)
    goto not_block_device;

  /* Block device ID is 0x2. */
  if (inl(reg_dev_id(d)) != 0x2)
    goto not_block_device;

  /* This string is hardcoded for debugging reasons.
     If virtio devices other than QEMU's are provided, we can remove this. */
  if (!compare_string_32(inl(reg_vend_id(d)), "QEMU"))
    goto not_block_device;

  d->is_blk = true;
  return true;

  not_block_device:
  d->is_blk = false;
  return false;
}

/* Identifies the device.  Registers the disk with the block device
   layer. */
static void identify_virtio_device(struct virtio_blk* d) {
  block_sector_t capacity;
  char vendor_id[5];
  uint32_t original_vendor_id;
  char extra_info[128];
  struct block* block;

  ASSERT(d->is_blk);

  /* Read capacity and vendor ID.
     To make it backward-compatible with x86 Pintos, capacity is 32-bit. */
  capacity = inl(conf_cap(d));
  original_vendor_id = inl(reg_vend_id(d));
  descramble_virtio_string(original_vendor_id, vendor_id);
  snprintf(extra_info, sizeof extra_info, "vendor ID \"%s\"", vendor_id);

  /* Disable access to disks over 1 GB, which are likely
     physical disks rather than virtual ones.  If we don't
     allow access to those, we're less likely to scribble on
     someone's important data.  You can disable this check by
     hand if you really want to do so. */
  if (capacity >= 1024 * 1024 * 1024 / BLOCK_SECTOR_SIZE) {
    #ifndef MACHINE
    printf("%s: ignoring ", d->name);
    print_human_readable_size(capacity * BLOCK_SECTOR_SIZE);
    printf("disk for safety\n");
    #endif
    d->is_blk = false;
    return;
  }

  /* Register. */
  block = block_register(d->name, BLOCK_RAW, extra_info,
                        capacity, &virtio_operations, d);
  partition_scan(block);
}

/* Translates SRC, which consists of 4 bytes, into a null-terminated 
   string in-place.  Returns in DEST.  */
static void descramble_virtio_string(uint32_t src, char* dest) {
  int i;

  for (i = 0; i < sizeof(src); ++i)
    dest[i] = ((char*) &src)[i];
  dest[sizeof(src)] = 0;
}

static void dma_alloc(struct virtio_blk* blk) {
  size_t desc_sz, avail_sz, used_sz, rw_size;

  desc_sz = desc_size(QUEUE_SIZE);
  avail_sz = avail_size(QUEUE_SIZE);
  used_sz = used_size(QUEUE_SIZE);

  /* We will read and write to desc and avail, but we will only read used. */
  rw_size = pg_round_up(desc_sz + avail_sz);
  used_sz = pg_round_up(used_sz);
  #ifdef MACHINE
  blk->desc = __M_mode_palloc(&next_avail_address, rw_size >> PGBITS);
  blk->avail = ((uintptr_t) blk->desc) + desc_sz;
  blk->used = __M_mode_palloc(&next_avail_address, used_sz >> PGBITS);
  #else
  blk->desc = palloc_get_multiple(PAL_ASSERT | PAL_ZERO, rw_size >> PGBITS);
  blk->avail = ((uintptr_t) blk->desc) + desc_sz;
  blk->used = palloc_get_multiple(PAL_ASSERT | PAL_ZERO, used_sz >> PGBITS);
  #endif
}

static inline uintptr_t _vtop(const void *vaddr) {
  #ifdef MACHINE
  return vaddr;
  #else
  return vtop(vaddr);
  #endif
}

/* Sets up the virtqueue for both the driver and the device. */
static void virtqueue_setup(struct virtio_blk* blk) {
  /* The followings are the steps from the spec:
     1. Select the queue writing its index (first queue is 0) to QueueSel.
     2. Check if the queue is not already in use: read QueueReady,
        and expect a returned value of zero (0x0).
     3. Read maximum queue size (number of elements) from QueueNumMax.
        If the returned value is zero (0x0) the queue is not available.
     4. Allocate and zero the queue memory, making sure the memory is
        physically contiguous.
     5. Notify the device about the queue size by writing the size to QueueNum.
     6. Write physical addresses of the queue’s Descriptor Area, Driver Area
        and Device Area to (respectively) the QueueDescLow/QueueDescHigh,
        QueueDriverLow/QueueDriverHigh and QueueDeviceLow/QueueDeviceHigh
        register pairs.
     7. Write 0x1 to QueueReady. */
  uint32_t queue_num_max;
  bool bit32 = sizeof(uintptr_t) == 4;

  outl(reg_queue_sel(blk), 0);

  if (inl(reg_queue_ready(blk)) != 0x0)
    PANIC("%s's queue 0 is already in use", blk->name);

  queue_num_max = inl(reg_queue_num_max(blk));
  if (queue_num_max == 0x0 || queue_num_max < QUEUE_SIZE)
    PANIC("Queue 0 of %s has bad QueueNumMax of %u", blk->name, queue_num_max);

  dma_alloc(blk);
  if (blk->mode == POLL) {
    blk->avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
    __sync_synchronize();
  }

  outl(reg_queue_num(blk), QUEUE_SIZE);

  /* The result of shifting by more than its bit width is undefined. */
  outl(reg_desc_low(blk), (uint32_t) (_vtop(blk->desc) & UINT32_MAX));
  outl(reg_desc_high(blk), (uint32_t) (bit32 ? 0 : _vtop(blk->desc) >> 32));
  outl(reg_drv_low(blk), (uint32_t) (_vtop(blk->avail) & UINT32_MAX));
  outl(reg_drv_high(blk), (uint32_t) (bit32 ? 0 : _vtop(blk->avail) >> 32));
  outl(reg_dev_low(blk), (uint32_t) (_vtop(blk->used) & UINT32_MAX));
  outl(reg_dev_high(blk), (uint32_t) (bit32 ? 0 : _vtop(blk->used) >> 32));

  outl(reg_queue_ready(blk), 0x1);
}

/* If WRITE is false, reads sector SEC_NO from disk D into BUFFER,
   which must have room for BLOCK_SECTOR_SIZE bytes.
   If WRITE is true, writes sector SEC_NO to disk D from BUFFER,
   which must contain BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to disks, so external
   per-disk locking is unneeded. */
static void virtio_blk_rw(struct virtio_blk* d, block_sector_t sec_no, void* buffer, bool write) {
  struct virtio_blk_req* req;
  uint16_t desc_idx, head;
  uint32_t id;

  #ifndef MACHINE
  lock_acquire(&d->lock);
  #endif

  /* In our design, we always allocate 3 */
  head = desc_idx = d->next_desc_idx;
  if (!alloc_while_busy(d, 3)) {
    PANIC("%s: disk %s failed, sector=%" PRDSNu, 
          d->name, write ? "write" : "read", sec_no);
  }

  /* From [virtio-v1.2] 2.7.13.1 "Placing Buffers Into The Descriptor Table":
     for each buffer element, b:
      1. Get the next free descriptor table entry, d
      2. Set d.addr to the physical address of the start of b
      3. Set d.len to the length of b.
      4. If b is device-writable, set d.flags to VIRTQ_DESC_F_WRITE, otherwise 0.
      5. If there is a buffer element after this:
        a. Set d.next to the index of the next free descriptor element.
        b. Set the VIRTQ_DESC_F_NEXT bit in d.flags. */

  /* The disk request. */
  req = &d->req[desc_idx];
  memset(req, 0, sizeof(struct virtio_blk_req));
  req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  req->sector = sec_no;
  write_desc(&d->desc[desc_idx], ((uintptr_t) req) & SIZE_MAX, sizeof(struct virtio_blk_req),
            VIRTQ_DESC_F_NEXT, desc_idx = (desc_idx + 1) % QUEUE_SIZE);
  
  /* Our request body. */
  write_desc(&d->desc[desc_idx], ((uintptr_t) buffer) & SIZE_MAX, BLOCK_SECTOR_SIZE,
            VIRTQ_DESC_F_NEXT | (write ? 0 : VIRTQ_DESC_F_WRITE),
            desc_idx = (desc_idx + 1) % QUEUE_SIZE);
  
  /* Device response. */
  write_desc(&d->desc[desc_idx], ((uintptr_t) &d->resp[desc_idx]) & SIZE_MAX,
            sizeof(struct virtio_blk_resp), VIRTQ_DESC_F_WRITE, 0);

  /* From [virtio-v1.2] 2.7.13 "Supplying Buffers to The Device":
     1. The driver places the buffer into free descriptor(s) in the descriptor
        table, chaining as necessary.
     2. The driver places the index of the head of the descriptor chain into the
        next ring entry of the available ring.
     3. Steps 1 and 2 MAY be performed repeatedly if batching is possible.
     4. The driver performs a suitable memory barrier to ensure the device sees
        the updated descriptor table and available ring before the next step.
     5. The available idx is increased by the number of descriptor chain heads
        added to the available ring.
     6. The driver performs a suitable memory barrier to ensure that it updates
        the idx field before checking for notification suppression.
     7. The driver sends an available buffer notification to the device if such
        notifications are not suppressed. */
  d->avail->ring[d->avail->idx % QUEUE_SIZE] = head;

  __sync_synchronize();

  /* From [virtio-v1.2] 2.7.6.1 "Driver Requirements: The Virtqueue Available
     Ring": A driver MUST NOT decrement the available idx on a virtqueue. */
  ++d->avail->idx;

  __sync_synchronize();

  if (!(d->used->flags & VIRTQ_USED_F_NO_NOTIFY))
    outl(reg_queue_notify(d), 0); /* Our queue is always 0. */

  if (d->mode == INTERRUPT) {
    #ifndef MACHINE
    sema_down(&d->completion_wait);
    #endif
    ASSERT(receive_pending(d));
  }
  else
    while(!receive_pending(d));
  
  /* We only recycle our earlier buffer here. */
  id = d->used->ring[d->last_seen_used % QUEUE_SIZE].id;
  ASSERT(id == head);
  ASSERT(d->resp[desc_idx].status == VIRTIO_BLK_S_OK);
  recycle_desc(d, id);
  ++d->last_seen_used;

  #ifndef MACHINE
  lock_release(&d->lock);
  #endif
}

/* Reads sector SEC_NO from disk D into BUFFER, which must have
   room for BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to disks, so external
   per-disk locking is unneeded. */
static void virtio_blk_read(void* d_, block_sector_t sec_no, void* buffer) {
  struct virtio_blk* d = d_;
  virtio_blk_rw(d, sec_no, buffer, false);
}

/* Write sector SEC_NO to disk D from BUFFER, which must contain
   BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to disks, so external
   per-disk locking is unneeded. */
static void virtio_blk_write(void* d_, block_sector_t sec_no, const void* buffer) {
  struct virtio_blk* d = d_;
  virtio_blk_rw(d, sec_no, buffer, true);
}

static struct block_operations virtio_operations = {virtio_blk_read, virtio_blk_write};

/* Low-level Virtio primitives. */

/* Write to disk DESC with the provided arguments.
   Convert ADDR to physical address if we are in Supervisor. */
static void write_desc(struct virtq_desc* desc, uint64_t addr, uint32_t len,
                      uint16_t flags, uint16_t next) {
  #ifndef MACHINE
  addr = vtop((void*) addr);
  #endif
  desc->addr = addr;
  desc->len = len;
  desc->flags = flags;
  desc->next = next;
}

/* Allocate CNT descriptors for disk D.
   These descriptors are always contiguous. From the spec:
   If VIRTIO_F_IN_ORDER has been negotiated, and when making a descriptor
   with VRING_DESC_F_NEXT set in flags at offset x in the table available
   to the device, driver MUST set next to 0 for the last descriptor in
   the table (where x = queue_size − 1) and to x + 1 for the rest of
   the descriptors.

   Return true if there is enough room, false otherwise. */
static bool alloc_desc(struct virtio_blk* d, size_t cnt) {
  if (d->in_used > QUEUE_SIZE - cnt)
    return false;
  d->next_desc_idx = (d->next_desc_idx + cnt) % QUEUE_SIZE;
  d->in_used += cnt;
  return true;
}

/* Recycle used descriptors with head at index IDX for disk D. */
static void recycle_desc(struct virtio_blk* d, uint16_t idx) {
  struct virtq_desc *desc, *next;

  desc = &d->desc[idx];
  while(desc != NULL) {
    desc->addr = 0x0;
    if (desc->flags & VIRTQ_DESC_F_NEXT)
      next = &d->desc[desc->next];
    else
      next = NULL;
    desc->flags = 0x0;
    desc->next = 0x0;
    desc = next;
    --d->in_used;
  }
}

/* Returns whether we can receive used buffer from the device. */
static bool receive_pending(struct virtio_blk* d) {
  __sync_synchronize();
  return d->last_seen_used != d->used->idx;
}

/* Wait up to 30 seconds for disk D to provide enough slots for CNT
   in the descriptor table. */
static bool alloc_while_busy(struct virtio_blk* d, size_t cnt) {
  int i;

  for (i = 0; i < 3000; i++) {
    if (i == 700)
      printf("%s: busy, waiting...", d->name);
    if (alloc_desc(d, 3)) {
      if (i >= 700)
        printf("ok\n");
      return true;
    }

    /* M-mode only uses virtio one time, and accesses data once per time
       in polling mode, so we should not need to wait. */
    #ifndef MACHINE
    timer_msleep(10);
    #endif
  }

  printf("failed\n");
  return false;
}

/* Selects the virtio block device according to IRQ. */
static inline struct virtio_blk* select_device(long irq) {
  return &blks[(size_t) irq - 1];
}

/* Virtio-blk nterrupt handler. */
static void interrupt_handler(struct intr_frame* f) {
  size_t idx_disk;
  uint32_t interrupt_status;
  struct virtio_blk* d;

  d = select_device(f->cause);
  interrupt_status = inl(reg_intr_status(d));
  ASSERT(interrupt_status != 0);
  outl(reg_intr_ack(d), interrupt_status);
  #ifndef MACHINE
  sema_up(&d->completion_wait);
  #endif
  return;
}
