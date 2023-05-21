/* Ensures that FPU is reset when jumping into a user program */

#include <stdint.h>
#include <float.h>
#include "tests/lib.h"
#include "tests/main.h"

#define FPU_SIZE 8 * 32 + 4

void test_main(void) {
  test_name = "fp-init";
  uint8_t fpu[FPU_SIZE];
  uint8_t init_fpu[FPU_SIZE];
  fsave(fpu);
  fninit();
  fsave(init_fpu);
  compare_bytes(&fpu, &init_fpu, FPU_SIZE, 0, test_name);
  msg("Success!");
  exit(162);
}
