/* Tests whether floating point registers are saved on kernel transitions */

#include <float.h>
#include "tests/userprog/kernel/tests.h"
#include "threads/thread.h"
#include "threads/synch.h"

#define NUM_VALUES 4
int values[NUM_VALUES] = {100, 101, 102, 103};

void fpu_write_up(void* args);
int fpu_write_read(int x);

struct fpu_args {
  int push_val;
  struct semaphore push_done;
};

/* Writes to FPU f0 its argument and ups a semaphore */
void fpu_write_up(void* args) {
  struct fpu_args* fpuargs = (struct fpu_args*)args;
  fpu_write(fpuargs->push_val, 0);
  sema_up(&fpuargs->push_done);
}

/* Writes integer x to FPU f0, yields the CPU, then pops it */
int fpu_write_read(int x) {
  fpu_write(x, 0);

  // Yield CPU and wait for thread to finish
  struct fpu_args fpuargs;
  fpuargs.push_val = x + 1;
  sema_init(&fpuargs.push_done, 0);
  thread_create("fpu-pusher", PRI_DEFAULT, &fpu_write_up, (void*)&fpuargs);
  sema_down(&fpuargs.push_done);

  return fpu_read(0);
}

void test_fp_kasm(void) {
  for (int i = 0; i < NUM_VALUES; i++) {
    int curr = values[i];
    int curr_pop = fpu_write_read(curr);
    if (curr != curr_pop)
      msg("Error: pushed %d but popped %d", curr, curr_pop);
  }
  pass();
}
