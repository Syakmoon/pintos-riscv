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

enum intr_level intr_get_level(void);
enum intr_level intr_set_level(enum intr_level);
enum intr_level intr_enable(void);
enum intr_level intr_disable(void);

/* Interrupt stack frame. */
// TEMP: do we need to save CSR_STATUS?
struct intr_frame {
  void (*epc)(void); /* Next instruction to execute. */

  unsigned long ra;
  unsigned long sp;
  unsigned long gp;
  unsigned long tp;
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

  unsigned long vec_no; /* Interrupt vector number. */
};

// struct intr_frame {
//   /* Pushed by intr_entry in intr-stubs.S.
//        These are the interrupted task's saved registers. */
//   uint32_t edi;       /* Saved EDI. */
//   uint32_t esi;       /* Saved ESI. */
//   uint32_t ebp;       /* Saved EBP. */
//   uint32_t esp_dummy; /* Not used. */
//   uint32_t ebx;       /* Saved EBX. */
//   uint32_t edx;       /* Saved EDX. */
//   uint32_t ecx;       /* Saved ECX. */
//   uint32_t eax;       /* Saved EAX. */
//   uint16_t gs, : 16;  /* Saved GS segment register. */
//   uint16_t fs, : 16;  /* Saved FS segment register. */
//   uint16_t es, : 16;  /* Saved ES segment register. */
//   uint16_t ds, : 16;  /* Saved DS segment register. */

//   /* Pushed by intrNN_stub in intr-stubs.S. */
//   uint32_t vec_no; /* Interrupt vector number. */

//   /* Sometimes pushed by the CPU,
//        otherwise for consistency pushed as 0 by intrNN_stub.
//        The CPU puts it just under `eip', but we move it here. */
//   uint32_t error_code; /* Error code. */

//   /* Pushed by intrNN_stub in intr-stubs.S.
//        This frame pointer eases interpretation of backtraces. */
//   void* frame_pointer; /* Saved EBP (frame pointer). */

//   /* Pushed by the CPU.
//        These are the interrupted task's saved registers. */
//   void (*eip)(void); /* Next instruction to execute. */
//   uint16_t cs, : 16; /* Code segment for eip. */
//   uint32_t eflags;   /* Saved CPU flags. */
//   void* esp;         /* Saved stack pointer. */
//   uint16_t ss, : 16; /* Data segment for esp. */
// };

typedef void intr_handler_func(struct intr_frame*);

void intr_init(void);
void intr_register_ext(uint8_t vec, intr_handler_func*, const char* name);
void intr_register_int(uint8_t vec, int dpl, enum intr_level, intr_handler_func*, const char* name);
bool intr_context(void);
void intr_yield_on_return(void);

void intr_dump_frame(const struct intr_frame*);
const char* intr_name(uint8_t vec);

#else

/* We align the sp to 16-byte for better performance. */
// #define INTR_FRAME_SIZE ALIGN(sizeof(struct intr_frame), 16)
#if __riscv_xlen == 32
#define INTR_FRAME_SIZE 144
#else
#define INTR_FRAME_SIZE 272
#endif /* __riscv_xlen */

#endif /* __ASSEMBLER__ */

#endif /* threads/interrupt.h */
