/* Pushes values to the FPU, then jumps into the kernel to perform a
   system call (which uses floating point arithmetic).
   Ensures that floating point registers are saved on a system call. */

#include <float.h>
#include <stdint.h>
#include <string.h>
#include <syscall-nr.h>
#include "tests/lib.h"
#include "tests/main.h"

#define FPU_SIZE 8 * 32 + 4
#define NUM_VALUES 32
static int values[NUM_VALUES] = {1, 6, 2, 162, 126, 2, 6, 1,
                                 1, 6, 2, 162, 126, 2, 6, 1,
                                 1, 6, 2, 162, 126, 2, 6, 1,
                                 1, 6, 2, 162, 126, 2, 6, 1};

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0)                                                                     \
  ({                                                                                               \
    int retval;                                                                                    \
    register uintptr_t a0 asm ("a0") = (uintptr_t)(NUMBER);                                        \
    register uintptr_t a1 asm ("a1") = (uintptr_t)(ARG0);                                          \
    asm volatile("ecall"                                                                           \
                 : "+r"(a0)                                                                        \
                 : "r"(a0), "r"(a1)                                                                \
                 : "memory");                                                                      \
    retval = a0;                                                                                   \
    retval;                                                                                        \
  })

void test_main(void) {
  test_name = "fp-syscall";
  struct fpu_regs fpu_before;
  struct fpu_regs fpu_after;

  msg("Computing e...");
  write_values_to_fpu(values, NUM_VALUES, 0);

  // Manually call the system call so that the compiler does not
  // generate FP instructions that modify the FPU in user space
  // Save FPU state before and after the syscall
  fsave(&fpu_before);
  int e_res = syscall1(SYS_COMPUTE_E, 10);
  fsave(&fpu_after);

  // Check if the FPU state is the same before and after the syscall
  // x86 Pintos ignore the Control Word (bytes 0-4) and the 
  // Tag Word (bytes 8-12) since those are modified by the FSAVE instruction
  // RISC-V does not do that
  compare_bytes(&fpu_before, &fpu_after, FPU_SIZE, 0, test_name);

  // RISC-V cannot assume this
  // if (read_values_from_fpu(values, NUM_VALUES, 0)) {
  //   msg("Success!");
  // } else {
  //   msg("Incorrect values popped");
  //   exit(126);
  // }

  // Convert the integer returned by the system call into a proper
  // double, so we can see if the return value is correct
  float e_res_flt;
  ASSERT(sizeof(float) == sizeof(int));
  memcpy(&e_res_flt, &e_res, sizeof(int));
  double e_res_dbl = (double)e_res_flt;
  if (abs_val(e_res_dbl - E_VAL) < TOL) {
    msg("Kernel computation successful");
    exit(162);
  } else {
    msg("Got e=%f, expected e=%f", e_res_dbl, E_VAL);
    exit(261);
  }
}
