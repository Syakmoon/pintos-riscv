#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H
#ifndef __ASSEMBLER__

#include <stdbool.h>
#include <stdint.h>

/* Interrupts on or off? */
enum intr_level {
  INTR_OFF, /* Interrupts disabled. */
  INTR_ON   /* Interrupts enabled. */
};

/* Interrupts. */
#define IRQ_S_SOFTWARE      1
#define IRQ_S_TIMER         5
#define IRQ_M_TIMER         7
#define IRQ_S_EXTERNAL      9
#define IRQ_M_EXTERNAL      11

/* Converted IRQs. */
#define INT_SSI             (1 << IRQ_S_SOFTWARE)
#define INT_STI             (1 << IRQ_S_TIMER)
#define INT_MTI             (1 << IRQ_M_TIMER)
#define INT_SEI             (1 << IRQ_S_EXTERNAL)

enum intr_level intr_get_level(void);
enum intr_level intr_set_level(enum intr_level);
enum intr_level intr_enable(void);
enum intr_level intr_disable(void);

/* Interrupt stack frame. */
struct intr_frame {
  void (*epc)(void); /* Next instruction to execute. */

  void* ra;
  void* sp;
  void* gp;
  void* tp;
  unsigned long t0;
  unsigned long t1;
  unsigned long t2;
  unsigned long s0;
  unsigned long s1;
  unsigned long a0;
  unsigned long a1;
  unsigned long a2;
  unsigned long a3;
  unsigned long a4;
  unsigned long a5;
  unsigned long a6;
  unsigned long a7;
  unsigned long s2;
  unsigned long s3;
  unsigned long s4;
  unsigned long s5;
  unsigned long s6;
  unsigned long s7;
  unsigned long s8;
  unsigned long s9;
  unsigned long s10;
  unsigned long s11;
  unsigned long t3;
  unsigned long t4;
  unsigned long t5;
  unsigned long t6;

  long cause; /* Trap cause number. */
  unsigned long trap_val; /* For page fault etc. */
  unsigned long status;
};

typedef void intr_handler_func(struct intr_frame*);

void intr_init(void);
void intr_register_ext(uint8_t vec, intr_handler_func*, const char* name);
void intr_register_int(uint8_t vec, bool exception, enum intr_level, intr_handler_func*, const char* name);
bool intr_context(void);
void intr_yield_on_return(void);

void intr_dump_frame(const struct intr_frame*);
const char* intr_name(long cause);

#endif /* __ASSEMBLER__ */

/* We align the sp to 16-byte for better performance. */
#if __riscv_xlen == 32
#define INTR_FRAME_SIZE 144
#else
#define INTR_FRAME_SIZE 272
#endif /* __riscv_xlen */

#endif /* threads/interrupt.h */
