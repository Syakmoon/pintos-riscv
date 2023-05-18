#include "devices/shutdown.h"
#include <console.h>
#include <stdio.h>
#include "devices/kbd.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#ifdef USERPROG
#include "userprog/exception.h"
#endif
#ifdef FILESYS
#include "devices/block.h"
#include "filesys/filesys.h"
#endif

/* Keyboard control register port. */
#define CONTROL_REG 0x64

/* Virtual address for QEMU to control the system. */
uint8_t* QEMU_mmio_address = MMIO_START + PGSIZE;

/* How to shut down when shutdown() is called. */
static enum shutdown_type how = SHUTDOWN_NONE;

static void print_stats(void);

#ifdef MACHINE
/* Shuts down the machine in the way configured by
   shutdown_configure().  If the shutdown type is SHUTDOWN_NONE
   (which is the default), returns without doing anything. */
void shutdown(void) {
  /* QEMU RISC-V power-off. */
  outw(QEMU_TEST_MMIO, 0x5555);

  /* None of those worked. */
  printf("still running...\n");
  for (;;)
    ;
}

#else

/* Shuts down the machine in the way configured by
   shutdown_configure().  If the shutdown type is SHUTDOWN_NONE
   (which is the default), returns without doing anything. */
void shutdown(void) {
  switch (how) {
    case SHUTDOWN_POWER_OFF:
      shutdown_power_off();
      break;

    case SHUTDOWN_REBOOT:
      shutdown_reboot();
      break;

    default:
      /* Nothing to do. */
      break;
  }
}

/* Sets TYPE as the way that machine will shut down when Pintos
   execution is complete. */
void shutdown_configure(enum shutdown_type type) { how = type; }

/* Reboots the machine via the keyboard controller. */
void shutdown_reboot(void) {
  printf("Rebooting...\n");

  /* See [kbd] for details on how to program the keyboard
     * controller. */
  // for (;;) {
  //   int i;

  //   /* Poll keyboard controller's status byte until
  //      * 'input buffer empty' is reported. */
  //   for (i = 0; i < 0x10000; i++) {
  //     if ((inb(CONTROL_REG) & 0x02) == 0)
  //       break;
  //     timer_udelay(2);
  //   }

  //   timer_udelay(50);

  //   /* Pulse bit 0 of the output port P2 of the keyboard controller.
  //      * This will reset the CPU. */
  //   outb(CONTROL_REG, 0xfe);
  //   timer_udelay(50);
  // }

  /* QEMU RISC-V reset */
  outw(QEMU_mmio_address, 0x7777);
}

/* Powers down the machine we're running on,
   as long as we're running on Bochs or QEMU. */
void shutdown_power_off(void) {
  const char s[] = "Shutdown";
  const char* p;

#ifdef FILESYS
  filesys_done();
#endif

  print_stats();

  printf("Powering off...\n");
  serial_flush();

  /* QEMU RISC-V power-off. */
  outw(QEMU_mmio_address, 0x5555);

  /* None of those worked. */
  printf("still running...\n");
  for (;;)
    ;
}

/* Print statistics about Pintos execution. */
static void print_stats(void) {
  timer_print_stats();
  thread_print_stats();
#ifdef FILESYS
  block_print_stats();
#endif
  console_print_stats();
//   kbd_print_stats();
#ifdef USERPROG
  exception_print_stats();
#endif
}

#endif
