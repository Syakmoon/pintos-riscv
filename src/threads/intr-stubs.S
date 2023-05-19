#include <riscv.h>
#include "threads/interrupt.h"

/* Reference: riscv-pk/pk/entry.S */

        .text
/* RISC-V requires TVEC to be 2^2 bytes aligned. */
.align 2
.globl intr_entry
.func intr_entry
intr_entry:
   /* If the trap is from kernel, sp will get 0. */
  csrrw sp, CSR_SCRATCH, sp
  bnez sp, save_context
  csrr sp, CSR_SCRATCH
save_context:
  /* Saves caller's registers. */
  addi sp, sp, -INTR_FRAME_SIZE
  REG_S x1,  1*REGBYTES(sp)
  REG_S x3,  3*REGBYTES(sp)
  REG_S x4,  4*REGBYTES(sp)
  REG_S x5,  5*REGBYTES(sp)
  REG_S x6,  6*REGBYTES(sp)
  REG_S x7,  7*REGBYTES(sp)
  REG_S x8,  8*REGBYTES(sp)
  REG_S x9,  9*REGBYTES(sp)
  REG_S x10, 10*REGBYTES(sp)
  REG_S x11, 11*REGBYTES(sp)
  REG_S x12, 12*REGBYTES(sp)
  REG_S x13, 13*REGBYTES(sp)
  REG_S x14, 14*REGBYTES(sp)
  REG_S x15, 15*REGBYTES(sp)
  REG_S x16, 16*REGBYTES(sp)
  REG_S x17, 17*REGBYTES(sp)
  REG_S x18, 18*REGBYTES(sp)
  REG_S x19, 19*REGBYTES(sp)
  REG_S x20, 20*REGBYTES(sp)
  REG_S x21, 21*REGBYTES(sp)
  REG_S x22, 22*REGBYTES(sp)
  REG_S x23, 23*REGBYTES(sp)
  REG_S x24, 24*REGBYTES(sp)
  REG_S x25, 25*REGBYTES(sp)
  REG_S x26, 26*REGBYTES(sp)
  REG_S x27, 27*REGBYTES(sp)
  REG_S x28, 28*REGBYTES(sp)
  REG_S x29, 29*REGBYTES(sp)
  REG_S x30, 30*REGBYTES(sp)
  REG_S x31, 31*REGBYTES(sp)

  /* Saves other registers. */
  csrr t0, CSR_EPC
  csrrw t1, CSR_SCRATCH, x0   /* This also ensures CSR_SCRATCH to be 0. */
  csrr t2, CSR_CAUSE
  csrr t3, CSR_TVAL           /* For page fault etc. */
  csrr t4, CSR_STATUS
  REG_S t0, 0*REGBYTES(sp)
  REG_S t1, 2*REGBYTES(sp)
  REG_S t2, 32*REGBYTES(sp)
  REG_S t3, 33*REGBYTES(sp)
  REG_S t4, 34*REGBYTES(sp)
  
  /* S2 is a callee-saved register, so we can safely assume it is unchanged. */
  csrr   s2, CSR_STATUS

  /* Put sp, which points to `struct intr_frame`, to argument 0. */
  mv a0, sp
  call intr_handler
.endfunc

.globl intr_exit
.func intr_exit
intr_exit:
  /* Loads other registers. */
  REG_L t0, 0*REGBYTES(sp)
  REG_L t4, 34*REGBYTES(sp)

  /* It is important to restore SSTATUS first since this disables interrupt,
     allowing SEPC to be unchanged.
     M-mode does not enable interrupt while handling traps, 
     therefore the order to restore does not matter. */
  csrw CSR_STATUS, t4
  csrw CSR_EPC, t0

  /* Restores caller's registers. */
  REG_L x1,  1*REGBYTES(sp)
  REG_L x3,  3*REGBYTES(sp)
  REG_L x4,  4*REGBYTES(sp)
  REG_L x5,  5*REGBYTES(sp)
  REG_L x6,  6*REGBYTES(sp)
  REG_L x7,  7*REGBYTES(sp)
  REG_L x8,  8*REGBYTES(sp)
  REG_L x9,  9*REGBYTES(sp)
  REG_L x10, 10*REGBYTES(sp)
  REG_L x11, 11*REGBYTES(sp)
  REG_L x12, 12*REGBYTES(sp)
  REG_L x13, 13*REGBYTES(sp)
  REG_L x14, 14*REGBYTES(sp)
  REG_L x15, 15*REGBYTES(sp)
  REG_L x16, 16*REGBYTES(sp)
  REG_L x17, 17*REGBYTES(sp)
  REG_L x19, 19*REGBYTES(sp)
  REG_L x20, 20*REGBYTES(sp)
  REG_L x21, 21*REGBYTES(sp)
  REG_L x22, 22*REGBYTES(sp)
  REG_L x23, 23*REGBYTES(sp)
  REG_L x24, 24*REGBYTES(sp)
  REG_L x25, 25*REGBYTES(sp)
  REG_L x26, 26*REGBYTES(sp)
  REG_L x27, 27*REGBYTES(sp)
  REG_L x28, 28*REGBYTES(sp)
  REG_L x29, 29*REGBYTES(sp)
  REG_L x30, 30*REGBYTES(sp)
  REG_L x31, 31*REGBYTES(sp)

  /* If we are trapping from kernel, do not restore CSR_SCRATCH. */
  #ifdef MACHINE
  srli s2, s2, MSTATUS_MPP_BIT
  andi s2, s2, 2
  #else
  andi s2, s2, SSTATUS_SPP
  #endif
  bnez s2, 1f

  addi sp, sp, INTR_FRAME_SIZE
  csrw CSR_SCRATCH, sp
  REG_L s2,  18*REGBYTES-INTR_FRAME_SIZE(sp)
  REG_L sp,  2*REGBYTES-INTR_FRAME_SIZE(sp)
  XRET
  j wfi_loop /* It should never return after xret */
1:
  REG_L s2,  18*REGBYTES(sp)
  addi sp, sp, INTR_FRAME_SIZE
  XRET
  j wfi_loop /* It should never return after xret */
.endfunc

wfi_loop:
  wfi
  j wfi_loop
