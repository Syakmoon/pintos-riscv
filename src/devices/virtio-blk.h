#ifndef DEVICES_VIRTIO_BLK_H
#define DEVICES_VIRTIO_BLK_H

/* Transmission mode. */
enum virtio_blk_mode { POLL, INTERRUPT };

void virtio_blks_init(enum virtio_blk_mode mode);

#endif /* devices/virtio-blk.h */
