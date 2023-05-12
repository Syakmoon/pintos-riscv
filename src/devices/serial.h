#ifndef DEVICES_SERIAL_H
#define DEVICES_SERIAL_H

#include <stdint.h>

#define SERIAL_MMIO_BASE 0x10000000L

void init_poll(void);
void serial_init_queue(void);
void serial_putc(uint8_t);
void serial_flush(void);
void serial_notify(void);

#endif /* devices/serial.h */
