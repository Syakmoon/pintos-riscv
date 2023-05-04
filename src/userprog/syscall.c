#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "userprog/exception.h"
#include "threads/thread.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(EXC_ECALL_U, true, INTR_ON, syscall_handler, "syscall"); }

static void syscall_handler(struct intr_frame* f UNUSED) {
  f->epc += 4;
  unsigned long args0 = ((uint32_t*)f->a0);
  unsigned long args1 = ((uint32_t*)f->a1);
  unsigned long args2 = ((uint32_t*)f->a2);
  unsigned long args3 = ((uint32_t*)f->a3);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (args0 == SYS_EXIT) {
    f->a0 = args1;
    // printf("%s: exit(%d)\n", thread_current()->pcb->process_name, args[1]);
    // process_exit();
  }
}
