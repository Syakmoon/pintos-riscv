#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <riscv.h>
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame*);
static void page_fault(struct intr_frame*);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void) {
  /* These exceptions can be raised explicitly by a user program. */
  intr_register_int(EXC_BREAKPOINT, true, INTR_ON, kill, "#BP Breakpoint");;

  /* These exceptions can be caused indirectly, 
     e.g. #DE can be caused by dividing by 0. */
  intr_register_int(EXC_INSTRUCTION_MISALIGNED, true, INTR_ON, kill, "#IM Instruction Address Misaligned");;
  intr_register_int(EXC_INSTRUCTION_FAULT, true, INTR_ON, kill, "#IF Instruction Access Fault");;
  intr_register_int(EXC_ILLEGAL_INSTRUCTION, true, INTR_ON, kill, "#IL Illegal Instruction");;
  intr_register_int(EXC_LOAD_MISALIGNED, true, INTR_ON, kill, "#LM Load Address Misaligned");;
  intr_register_int(EXC_LOAD_FAULT, true, INTR_ON, kill, "#LF Load Access Fault");;
  intr_register_int(EXC_STORE_MISALIGNED, true, INTR_ON, kill, "#SM Store/AMO Address Misaligned");;
  intr_register_int(EXC_STORE_FAULT, true, INTR_ON, kill, "#SF Store/AMO Access Fault");;
  
  /* We do not allow ecall in S-mode or M-mode in this implementation. */
  intr_register_int(EXC_ECALL_S, true, INTR_ON, kill, "#ES Environment Call From S-mode");;
  intr_register_int(EXC_ECALL_M, true, INTR_ON, kill, "#EM Environment Call From M-mode");;

  /* Page faults. This is different from the original x86 Pintos in that
     we can turn on interrupt for page faults. */
  intr_register_int(EXC_INSTRUCTION_PAGE_FAULT, true, INTR_ON, page_fault, "#PF Instruction Page Fault");
  intr_register_int(EXC_LOAD_PAGE_FAULT, true, INTR_ON, page_fault, "#PF Load Page Fault");
  intr_register_int(EXC_STORE_PAGE_FAULT, true, INTR_ON, page_fault, "#PF Store/AMO Page Fault");
}

/* Prints exception statistics. */
void exception_print_stats(void) { printf("Exception: %lld page faults\n", page_fault_cnt); }

/* Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame* f) {
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

  /* The SPP tells us where the exception originated. */
    if (!(f->status & SSTATUS_SPP)) {
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf("%s: dying due to cause %#04x (%s).\n", thread_name(), f->cause,
             intr_name(f->cause));
      intr_dump_frame(f);
      process_exit();
      NOT_REACHED();
    }
    else {
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame(f);
      PANIC("Kernel bug - unexpected interrupt in kernel");
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void page_fault(struct intr_frame* f) {
  char* cause;      /* The cause string. */
  bool user;        /* True: access by user, false: access by kernel. */
  void* fault_addr; /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  fault_addr = f->trap_val;

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  cause = f->cause == 12 ? "fetching" : f->cause == 13 ? "loading" :
         f->cause == 15 ? "storing" : "unknown action to";
  user = (f->status & SSTATUS_SPP) == 0;

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf("Page fault at %p: %s page in %s context.\n", fault_addr,
         cause, user ? "user" : "kernel");
  kill(f);
}
