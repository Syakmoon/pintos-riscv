#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

#define CLINT 0x2000000L
#define CLINT_MTIME (CLINT + 0xbff8)
#define CLINT_MTIMECMP (CLINT + 0x4000) /* We have only one HART. */

/* 10ms on QEMU emulation */
#define TIMER_INTERVAL 1000000  // TODO: change to some other approaches

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

void timer_init(void);
void timer_calibrate(void);

int64_t timer_ticks(void);
int64_t timer_elapsed(int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep(int64_t ticks);
void timer_msleep(int64_t milliseconds);
void timer_usleep(int64_t microseconds);
void timer_nsleep(int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay(int64_t milliseconds);
void timer_udelay(int64_t microseconds);
void timer_ndelay(int64_t nanoseconds);

void timer_print_stats(void);

#endif /* devices/timer.h */
