#include "threads/switch.h"
#include <riscv.h>

#### struct thread *switch_threads (struct thread *cur, struct thread *next);
####
#### Switches from CUR, which must be the running thread, to NEXT,
#### which must also be running switch_threads(), returning CUR in
#### NEXT's context.
####
#### This function works by assuming that the thread we're switching
#### into is also running switch_threads().  Thus, all it has to do is
#### preserve a few registers on the stack, then switch stacks and
#### restore the registers.  As part of switching stacks we record the
#### current stack pointer in CUR's thread structure.


.globl switch_threads
.func switch_threads
switch_threads:
	# Save caller's register state.
	#
	# We save all callee saved registers, and RA.
	#
	# This stack frame must match the one set up by thread_create()
	# in size.
	addi sp, sp, -SWITCH_THREADS_FRAME_SIZE
  	REG_S s0, 0*REGBYTES(sp)
  	REG_S s1, 1*REGBYTES(sp)
  	REG_S s2, 2*REGBYTES(sp)
  	REG_S s3, 3*REGBYTES(sp)
  	REG_S s4, 4*REGBYTES(sp)
  	REG_S s5, 5*REGBYTES(sp)
  	REG_S s6, 6*REGBYTES(sp)
  	REG_S s7, 7*REGBYTES(sp)
  	REG_S s8, 8*REGBYTES(sp)
  	REG_S s9, 9*REGBYTES(sp)
  	REG_S s10, 10*REGBYTES(sp)
  	REG_S s11, 11*REGBYTES(sp)
  	REG_S ra, 12*REGBYTES(sp)

	# Get offsetof (struct thread, stack).
.globl thread_stack_ofs
	la t2, thread_stack_ofs
	REG_L t2, 0(t2)

	# T0 and T1 are CUR and NEXT's stack pointers, respectively.
	add t0, a0, t2
	add t1, a1, t2

	# Save current stack pointer to old thread's stack, if any.
	REG_S sp, 0(t0)

	# Restore stack pointer from new thread's stack.
	REG_L sp, 0(t1)

	# Restore caller's register state.
  	REG_L s0, 0*REGBYTES(sp)
  	REG_L s1, 1*REGBYTES(sp)
  	REG_L s2, 2*REGBYTES(sp)
  	REG_L s3, 3*REGBYTES(sp)
  	REG_L s4, 4*REGBYTES(sp)
  	REG_L s5, 5*REGBYTES(sp)
  	REG_L s6, 6*REGBYTES(sp)
  	REG_L s7, 7*REGBYTES(sp)
  	REG_L s8, 8*REGBYTES(sp)
  	REG_L s9, 9*REGBYTES(sp)
  	REG_L s10, 10*REGBYTES(sp)
  	REG_L s11, 11*REGBYTES(sp)
  	REG_L ra, 12*REGBYTES(sp)
	addi sp, sp, SWITCH_THREADS_FRAME_SIZE
        ret
.endfunc

.globl switch_entry
.func switch_entry
switch_entry:
	# Call thread_switch_tail(prev).
.globl thread_switch_tail
	call thread_switch_tail

	# Start thread proper.
	REG_L ra, 0*REGBYTES(sp)
	addi sp, sp, REGBYTES
	REG_L a0, 0*REGBYTES(sp)
	REG_L a1, 1*REGBYTES(sp)
	ret
.endfunc
