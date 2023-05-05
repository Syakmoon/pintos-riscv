#include <riscv.h>
#include "threads/interrupt.h"

/* Reference: riscv-pk/pk/entry.S */

        .text

.macro save_gpr
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
.endm

.macro load_gpr
  /* Restores caller's registers. */
  REG_L x1,  1*REGBYTES(sp)
  REG_L x3,  3*REGBYTES(sp)
  REG_L x4,  4*REGBYTES(sp)
  REG_L x5,  5*REGBYTES(sp)
  REG_L x6,  6*REGBYTES(sp)
  REG_L x7,  7*REGBYTES(sp)
  REG_L x9,  9*REGBYTES(sp)
  REG_L x10, 10*REGBYTES(sp)
  REG_L x11, 11*REGBYTES(sp)
  REG_L x12, 12*REGBYTES(sp)
  REG_L x13, 13*REGBYTES(sp)
  REG_L x14, 14*REGBYTES(sp)
  REG_L x15, 15*REGBYTES(sp)
  REG_L x16, 16*REGBYTES(sp)
  REG_L x17, 17*REGBYTES(sp)
  REG_L x18, 18*REGBYTES(sp)
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

.endm

/* RISC-V requires TVEC to be 2^2 bytes aligned. */
.align 2
.globl mintr_entry
.func mintr_entry
mintr_entry:
   /* If the trap is from kernel, sp will get 0. */
  csrrw sp, mscratch, sp
  bnez sp, msave_context
  csrr sp, mscratch
msave_context:
  save_gpr

  /* Saves other registers. */
  csrr t0, mepc
  csrrw t1, mscratch, x0   /* This also ensures CSR_SCRATCH to be 0. */
  csrr t2, mcause
  csrr t3, mtval  /* For page fault etc. */
  csrr t4, mstatus
  REG_S t0, 0*REGBYTES(sp)
  REG_S t1, 2*REGBYTES(sp)
  REG_S t2, 32*REGBYTES(sp)
  REG_S t3, 33*REGBYTES(sp)
  REG_S t4, 34*REGBYTES(sp)
  
  /* s0 is a callee-saved register, so we can safely assume it is unchanged. */
  csrr   s0,mstatus

  /* Put sp, which points to `struct intr_frame`, to argument 0. */
  mv a0, sp
  jal intr_handler
.endfunc

.globl mintr_exit
.func mintr_exit
mintr_exit:
  /* Loads other registers. */
  REG_L t0, 0*REGBYTES(sp)
  REG_L t4, 34*REGBYTES(sp)
  csrw mepc, t0

  /* M-mode does not enable interrupt while handling traps, 
     therefore the order to restore does not matter. */
  csrw mstatus, t4

  load_gpr

  /* If we are trapping from kernel (Machine), do not restore CSR_SCRATCH. */
  srli s0, s0, MSTATUS_MPP_BIT
  andi s0, s0, 2
  bnez s0, 1f
  addi sp, sp, INTR_FRAME_SIZE
  csrw mscratch, sp
  REG_L s0,  8*REGBYTES-INTR_FRAME_SIZE(sp)
  REG_L sp,  2*REGBYTES-INTR_FRAME_SIZE(sp)
  mret
  j wfi_loop /* It should never return after mret */
1:
  REG_L s0,  8*REGBYTES(sp)
  addi sp, sp, INTR_FRAME_SIZE
  mret
  j wfi_loop /* It should never return after mret */
.endfunc

.align 2
.globl intr_entry
.func intr_entry
intr_entry:
   /* If the trap is from kernel, sp will get 0. */
  csrrw sp, sscratch, sp
  bnez sp, save_context
  csrr sp, sscratch
save_context:
  save_gpr

  /* Saves other registers. */
  csrr t0, sepc
  csrrw t1, sscratch, x0   /* This also ensures CSR_SCRATCH to be 0. */
  csrr t2, scause
  csrr t3, stval  /* For page fault etc. */
  csrr t4, sstatus
  REG_S t0, 0*REGBYTES(sp)
  REG_S t1, 2*REGBYTES(sp)
  REG_S t2, 32*REGBYTES(sp)
  REG_S t3, 33*REGBYTES(sp)
  REG_S t4, 34*REGBYTES(sp)
  
  /* s0 is a callee-saved register, so we can safely assume it is unchanged. */
  csrr   s0,sstatus

  /* Put sp, which points to `struct intr_frame`, to argument 0. */
  mv a0, sp
  jal intr_handler
.endfunc

.globl intr_exit
.func intr_exit
intr_exit:
  /* Loads other registers. */
  REG_L t0, 0*REGBYTES(sp)
  REG_L t4, 34*REGBYTES(sp)

  /* It is important to restore SSTATUS first since this disables interrupt,
     allowing SEPC to be unchanged. */
  csrw sstatus, t4
  csrw sepc, t0

  load_gpr

  /* If we are trapping from kernel (Supervisor), do not restore CSR_SCRATCH. */
  andi s0, s0, SSTATUS_SPP
  bnez s0, 1f
  addi sp, sp, INTR_FRAME_SIZE
  csrw sscratch, sp
  REG_L s0,  8*REGBYTES-INTR_FRAME_SIZE(sp)
  REG_L sp,  2*REGBYTES-INTR_FRAME_SIZE(sp)
  sret
  j wfi_loop /* It should never return after sret */
1:
  REG_L s0,  8*REGBYTES(sp)
  addi sp, sp, INTR_FRAME_SIZE
  sret
  j wfi_loop /* It should never return after sret */
.endfunc

wfi_loop:
  wfi
  j wfi_loop
