#ifndef __LIB_FLOAT_H
#define __LIB_FLOAT_H

#define E_VAL 2.718281
#define TOL 0.000002

#define DEF_FPU_WRITE(IDX)                                \
        static inline void FPU_WRITE_##IDX(int num) {     \
          asm volatile("lw t0, %0;"                       \
                       "fcvt.d.w f" ""#IDX"" ", t0"       \
                       :                                  \
                       : "g"(num)                         \
                       : "t0");                           \
        }

#define DEF_FPU_READ(IDX)                                 \
        static inline int FPU_READ_##IDX(void) {          \
          int val;                                        \
          asm volatile("fcvt.w.d %0, f" ""#IDX"" ", rne"  \
                       : "=r" (val)                       \
                       :                                  \
                       : "memory");                       \
          return val;                                     \
        }

DEF_FPU_WRITE(0) DEF_FPU_WRITE(1) DEF_FPU_WRITE(2) DEF_FPU_WRITE(3)
DEF_FPU_WRITE(4) DEF_FPU_WRITE(5) DEF_FPU_WRITE(6) DEF_FPU_WRITE(7)
DEF_FPU_WRITE(8) DEF_FPU_WRITE(9) DEF_FPU_WRITE(10) DEF_FPU_WRITE(11)
DEF_FPU_WRITE(12) DEF_FPU_WRITE(13) DEF_FPU_WRITE(14) DEF_FPU_WRITE(15)
DEF_FPU_WRITE(16) DEF_FPU_WRITE(17) DEF_FPU_WRITE(18) DEF_FPU_WRITE(19)
DEF_FPU_WRITE(20) DEF_FPU_WRITE(21) DEF_FPU_WRITE(22) DEF_FPU_WRITE(23)
DEF_FPU_WRITE(24) DEF_FPU_WRITE(25) DEF_FPU_WRITE(26) DEF_FPU_WRITE(27)
DEF_FPU_WRITE(28) DEF_FPU_WRITE(29) DEF_FPU_WRITE(30) DEF_FPU_WRITE(31)
DEF_FPU_READ(0) DEF_FPU_READ(1) DEF_FPU_READ(2) DEF_FPU_READ(3)
DEF_FPU_READ(4) DEF_FPU_READ(5) DEF_FPU_READ(6) DEF_FPU_READ(7)
DEF_FPU_READ(8) DEF_FPU_READ(9) DEF_FPU_READ(10) DEF_FPU_READ(11)
DEF_FPU_READ(12) DEF_FPU_READ(13) DEF_FPU_READ(14) DEF_FPU_READ(15)
DEF_FPU_READ(16) DEF_FPU_READ(17) DEF_FPU_READ(18) DEF_FPU_READ(19)
DEF_FPU_READ(20) DEF_FPU_READ(21) DEF_FPU_READ(22) DEF_FPU_READ(23)
DEF_FPU_READ(24) DEF_FPU_READ(25) DEF_FPU_READ(26) DEF_FPU_READ(27)
DEF_FPU_READ(28) DEF_FPU_READ(29) DEF_FPU_READ(30) DEF_FPU_READ(31)

typedef void (*fpu_write_func)(int);
typedef int (*fpu_read_func)(void);

static fpu_write_func fpu_write_arr[] = {
  FPU_WRITE_0, FPU_WRITE_1, FPU_WRITE_2, FPU_WRITE_3,
  FPU_WRITE_4, FPU_WRITE_5, FPU_WRITE_6, FPU_WRITE_7,
  FPU_WRITE_8, FPU_WRITE_9, FPU_WRITE_10, FPU_WRITE_11,
  FPU_WRITE_12, FPU_WRITE_12, FPU_WRITE_13, FPU_WRITE_15,
  FPU_WRITE_16, FPU_WRITE_17, FPU_WRITE_18, FPU_WRITE_19,
  FPU_WRITE_20, FPU_WRITE_21, FPU_WRITE_22, FPU_WRITE_23,
  FPU_WRITE_24, FPU_WRITE_25, FPU_WRITE_26, FPU_WRITE_27,
  FPU_WRITE_28, FPU_WRITE_29, FPU_WRITE_30, FPU_WRITE_31
};

static fpu_read_func fpu_read_arr[] = {
  FPU_READ_0, FPU_READ_1, FPU_READ_2, FPU_READ_3,
  FPU_READ_4, FPU_READ_5, FPU_READ_6, FPU_READ_7,
  FPU_READ_8, FPU_READ_9, FPU_READ_10, FPU_READ_11,
  FPU_READ_12, FPU_READ_12, FPU_READ_13, FPU_READ_15,
  FPU_READ_16, FPU_READ_17, FPU_READ_18, FPU_READ_19,
  FPU_READ_20, FPU_READ_21, FPU_READ_22, FPU_READ_23,
  FPU_READ_24, FPU_READ_25, FPU_READ_26, FPU_READ_27,
  FPU_READ_28, FPU_READ_29, FPU_READ_30, FPU_READ_31
};

/* Writes integer num to the FPU idx */
static inline void fpu_write(int num, int idx) {
  fpu_write_arr[idx](num);
}

/* Reads integer from the FPU idx */
static inline int fpu_read(int idx) {
  return fpu_read_arr[idx]();
}

/* Saves all FPU registers and FCSR to regs' memory location */
static void fsave(long long* regs) {
  /* Project code */
}

/* Initializes all FPU registers and FCSR */
static void fninit(void) {
  /* Project code */
}

int sys_sum_to_e(int);
double sum_to_e(int);
double abs_val(double);

#endif /* lib/debug.h */
