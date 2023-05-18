#ifndef THREADS_SWITCH_H
#define THREADS_SWITCH_H

#ifndef __ASSEMBLER__
/* switch_thread()'s stack frame. */
struct switch_threads_frame {
  unsigned long s0;    /*  0*REGBYTES: Saved S0/FP. */
  unsigned long s1;    /*  1*REGBYTES: Saved S1. */
  unsigned long s2;    /*  2*REGBYTES: Saved S2. */
  unsigned long s3;    /*  3*REGBYTES: Saved S3. */
  unsigned long s4;    /*  4*REGBYTES: Saved S4. */
  unsigned long s5;    /*  5*REGBYTES: Saved S5. */
  unsigned long s6;    /*  6*REGBYTES: Saved S6. */
  unsigned long s7;    /*  7*REGBYTES: Saved S7. */
  unsigned long s8;    /*  8*REGBYTES: Saved S8. */
  unsigned long s9;    /*  9*REGBYTES: Saved S9. */
  unsigned long s10;   /*  10*REGBYTES: Saved S10. */
  unsigned long s11;   /*  11*REGBYTES: Saved S11. */
  void (*ra)(void);    /*  12*REGBYTES: Return address. */
};

/* Switches from CUR, which must be the running thread, to NEXT,
   which must also be running switch_threads(), returning CUR in
   NEXT's context. */
struct thread* switch_threads(struct thread* cur, struct thread* next);

/* Stack frame for switch_entry(). */
struct switch_entry_frame {
  void (*ra)(void);
};

void switch_entry(void);

/* Pops the CUR and NEXT arguments off the stack, for use in
   initializing threads. */
void switch_thunk(void);
#endif

/* Offsets used by switch.S. */
#if __riscv_xlen == 32
#define SWITCH_THREADS_FRAME_SIZE 52
#else
#define SWITCH_THREADS_FRAME_SIZE 104
#endif /* __riscv_xlen */

#endif /* threads/switch.h */
