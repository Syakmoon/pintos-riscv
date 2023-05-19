/* Expand the stack by 32 bytes all at once using the PUSHA
   instruction.
   This must succeed. */

#include <riscv.h>
#include <string.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  asm volatile("mv t0, sp;"               /* Save a copy of the stack pointer. */
               "li t1, 0xfffff000;"
               "and sp, sp, t1;"          /* Move stack pointer to bottom of page. */
               XSTR(REG_S) " t0, -32(sp);"/* Push 32 bytes on stack at once. */
               "mv sp, t0"                /* Restore copied stack pointer. */
               :
               :
               : "t0", "t1"); /* Tell GCC we destroyed T0, T1. */
}
