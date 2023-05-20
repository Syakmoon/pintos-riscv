#include "devices/rtc.h"
#include <stdio.h>
#include <riscv.h>
#include "threads/io.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

/* This code is an interface to the MC146818A-compatible real
   time clock found on PC motherboards.  See [MC146818A] for
   hardware details. */

/* I/O register address. */
uintptr_t RTC_BASE = 0x101000;  /* Physical address of Goldfish RTC. */

/* Indexes of RTC registers.*/
#define RTC_TIME_LOW  0x00  /* Second low. */
#define RTC_TIME_HIGH 0x04  /* Second high. */

#define NSEC_PER_SECOND 1000000000L

/* Returns number of seconds since Unix epoch of January 1,
   1970. */
time_t rtc_get_time(void) {
  unsigned int low, high;
  time_t time, nsec;
  unsigned int* nsec_high_ptr;
  uintptr_t* old_page_dir;

  nsec_high_ptr = &nsec;
  ++nsec_high_ptr;

  /* init_page_dir has not been initialized yet. */
  old_page_dir = ptov((csr_read(CSR_SATP) & SATP_PPN) << PGBITS);

  /* Pintos only needs to get the time, so we set it to read-only. */
  if (RTC_BASE == 0x101000)
    RTC_BASE = pagedir_set_mmio(old_page_dir, RTC_BASE, 0x1000, false);

  /* Get time components. */
  low = inl(RTC_BASE + RTC_TIME_LOW);
  high = inl(RTC_BASE + RTC_TIME_HIGH);

  /* Break down all components into seconds. */
  nsec = low;
  *nsec_high_ptr = high;
  time = nsec / NSEC_PER_SECOND;

  return time;
}
