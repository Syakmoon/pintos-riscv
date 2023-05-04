#include "threads/interrupt.h"
#include <debug.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <riscv.h>
#include "threads/intr-stubs.h"
#include "threads/io.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/timer.h"

/* Platform-Level Interrupt Controller (PLIC) MMIO.
   https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc */
#define PLIC             0x0c000000L
#define PLIC_PRIORITY    PLIC
#define PLIC_PENDING     (PLIC + 0x1000)
#define PLIC_S_ENABLE    (PLIC + 0x2080)
#define PLIC_M_PRIORITY  (PLIC + 0x200000)
#define PLIC_S_PRIORITY  (PLIC + 0x201000)
#define PLIC_S_CLAIM     (PLIC + 0x201004)
#define PLIC_S_COMPLETE  PLIC_S_CLAIM

/* Number of x86 interrupts. */
#define INTR_CNT 256

/* 0-127: exceptions. 128-: external exceptions. -255: interrupts. */

/* Interrupt handler functions for each interrupt. */
static intr_handler_func* intr_handlers[INTR_CNT];

/* Names for each interrupt, for debugging purposes. */
static const char* intr_names[INTR_CNT];

/* Number of unexpected interrupts for each vector.  An
   unexpected interrupt is one that has no registered handler. */
static unsigned int unexpected_cnt[INTR_CNT];

/* External interrupts are those generated by devices outside the
   CPU, such as the timer.  External interrupts run with
   interrupts turned off, so they never nest, nor are they ever
   pre-empted.  Handlers for external interrupts also may not
   sleep, although they may invoke intr_yield_on_return() to
   request that a new process be scheduled just before the
   interrupt returns. */
static bool in_external_intr; /* Are we processing an external interrupt? */
static bool yield_on_return;  /* Should we yield on interrupt return? */

/* Platform-Level Interrupt Controller helpers. */
static void plic_init(void);
static void plic_register(int irq, uint32_t priority);
static int plic_claim(void);
static void plic_end_of_interrupt(int irq);

/* Interrupt handlers. */
void intr_handler(struct intr_frame* args);
static void unexpected_interrupt(const struct intr_frame*);

/* Returns the current interrupt status. Only for Supervisor */
enum intr_level intr_get_level(void) {
  unsigned long sstatus;
  sstatus = csr_read(CSR_SSTATUS);
  return sstatus & SSTATUS_SIE ? INTR_ON : INTR_OFF;
}

/* Enables or disables interrupts as specified by LEVEL and
   returns the previous interrupt status. */
enum intr_level intr_set_level(enum intr_level level) {
  return level == INTR_ON ? intr_enable() : intr_disable();
}

/* Enables interrupts and returns the previous interrupt status. */
enum intr_level intr_enable(void) {
  enum intr_level old_level = intr_get_level();
  ASSERT(!intr_context());

  /* Enable Supervisor interrupts by setting the SIE flag. */
  csr_write(CSR_SSTATUS, csr_read(CSR_SSTATUS) | SSTATUS_SIE);

  return old_level;
}

/* Disables interrupts and returns the previous interrupt status. */
enum intr_level intr_disable(void) {
  enum intr_level old_level = intr_get_level();

  /* Disable Supervisor interrupts by clearing the SIE flag. */
  csr_write(CSR_SSTATUS, csr_read(CSR_SSTATUS) & ~SSTATUS_SIE);

  return old_level;
}

static inline uint8_t to_real_vec_no(uint8_t vec_no, bool exception, bool external) {
  if (external)
    return vec_no + INTR_CNT / 2;
  else if (!exception)
    return INTR_CNT - vec_no - 1;
  return vec_no;
}

static uint8_t cause_to_vec_no(long cause) {
  if (cause < 0) {
    if ((cause & 0xf) >= 8 && (cause & 0xf) <= 11)
      return to_real_vec_no(plic_claim(), false, true);
  }
  return to_real_vec_no(cause & INTPTR_MAX, cause < 0, false);
}

/* Initializes the interrupt system. */
void intr_init(void) {
  uint64_t idtr_operand;
  int i;

  /* Initialize interrupt controller. */
  plic_init();

  /* Load Supervisor trap vector. */
  // csr_write(CSR_STVEC, intr_entry);

  /* Initialize intr_names. */
  for (i = 0; i < INTR_CNT; i++)
    intr_names[i] = "unknown";
}

/* Registers interrupt VEC_NO to invoke HANDLER.
   Names the interrupt NAME for debugging
   purposes.  Level is here only to determine external or not. */
static void register_handler(uint8_t vec_no, bool exception, enum intr_level level,
                             intr_handler_func* handler, const char* name) {
  uint8_t real_vec_no = to_real_vec_no(vec_no, exception, level == INTR_OFF);
  ASSERT(intr_handlers[real_vec_no] == NULL);
  
  /* If this is an external interrupt, register it in the PLIC. */
  if (level == INTR_OFF) {
    plic_register(vec_no, 1);
  }

  intr_handlers[real_vec_no] = handler;
  intr_names[real_vec_no] = name;
}

/* Registers external interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The handler will
   execute with interrupts disabled. */
void intr_register_ext(uint8_t vec_no, intr_handler_func* handler, const char* name) {
  register_handler(vec_no, false, INTR_OFF, handler, name);
}

/* Registers internal interrupt VEC_NO to invoke HANDLER, which
   is named NAME for debugging purposes.  The interrupt handler
   will be invoked with interrupt status LEVEL.

   The handler will have descriptor privilege level DPL, meaning
   that it can be invoked intentionally when the processor is in
   the DPL or lower-numbered ring.  In practice, DPL==3 allows
   user mode to invoke the interrupts and DPL==0 prevents such
   invocation.  Faults and exceptions that occur in user mode
   still cause interrupts with DPL==0 to be invoked.  See
   [IA32-v3a] sections 4.5 "Privilege Levels" and 4.8.1.1
   "Accessing Nonconforming Code Segments" for further
   discussion. */
void intr_register_int(uint8_t vec_no, bool exception, enum intr_level level, intr_handler_func* handler,
                       const char* name) {
  register_handler(vec_no, exception, level, handler, name);
}

/* Returns true during processing of an external interrupt
   and false at all other times. */
bool intr_context(void) { return in_external_intr; }

/* During processing of an external interrupt, directs the
   interrupt handler to yield to a new process just before
   returning from the interrupt.  May not be called at any other
   time. */
void intr_yield_on_return(void) {
  ASSERT(intr_context());
  yield_on_return = true;
}

/* Platform-Level Interrupt Controller. */

/* Initializes the PLIC. */
static void plic_init(void) {
  /* Set Supervisor and Machine priority threshold to 0 and 1.
     The PLIC will mask all PLIC interrupts of a priority less than 
     or equal to threshold.
     This again ensures Supervisor handles all external interrupts. */
  outl(PLIC_S_PRIORITY, 0);
  outl(PLIC_M_PRIORITY, 1);

  /* Unmask all interrupts. */
  // TEMP: this is just for debugging, now?
  outl(PLIC_S_ENABLE, (uint32_t) -1);
}

/* Enables the corresponding IRQ for Supervisor 
   and sets its priority to non-zero. */
static void plic_register(int irq, uint32_t prioirty) {
  /* RISC-V PLIC supports at most 1024 interrupt sources.
     For our purpose, we only support 64 sources to avoid overflows. */
  ASSERT(irq >= 0 && irq < 64);

  /* A priority value of 0 is reserved to mean "never interrupt" 
     and effectively disables the interrupt. 
     Priority 1 is the lowest active priority while 
     the maximum level of priority depends on PLIC implementation. */
  outl(PLIC_PRIORITY + irq * 4, prioirty);

  /* Each global interrupt can be enabled by setting 
     the corresponding bit in the enables register. 
     The enables registers are accessed as a contiguous array of 
     32-bit registers, packed the same way as the PENDING bits.
     The pending bit for interrupt ID N is stored in 
     bit (N mod 32) of word (N/32). */
  outl(PLIC_S_ENABLE, inl(PLIC_S_ENABLE) | (1 << irq));
}

/* Claims the interrupt from the PLIC to get IRQ. */
static int plic_claim(void) {
  /* The PLIC can perform an interrupt claim by reading 
     the CLAIM/COMPLETE register, which returns the ID of 
     the highest priority pending interrupt or zero 
     if there is no pending interrupt. 
     A successful claim will also atomically clear 
     the corresponding pending bit on the interrupt source. */
  return inl(PLIC_S_CLAIM);
}

/* The PLIC signals it has completed executing an interrupt handler 
   by writing the interrupt ID it received 
   from the claim to the CLAIM/COMPLETE register. 
   The PLIC does not check whether the completion ID is the same as 
   the last claim ID for that target. If the completion ID does not match 
   an interrupt source that is currently enabled for the target, 
   the completion is silently ignored.  */
static void plic_end_of_interrupt(int irq) {
  ASSERT(irq >= 0 && irq < 64);

  /* Acknowledge the IRQ. */
  outl(PLIC_S_COMPLETE, irq);
}

/* Returns true if this trap to the OS was from userspace */
#ifdef USERPROG
static inline bool is_trap_from_userspace(struct intr_frame* frame) {
  return !(csr_read(CSR_SSTATUS) & SSTATUS_SPP);
}
#endif

/* Interrupt handlers. */

/* Handler for all interrupts, faults, and exceptions.  This
   function is called by the assembly language interrupt stubs in
   intr-stubs.S.  FRAME describes the interrupt and the
   interrupted thread's registers. */
void intr_handler(struct intr_frame* frame) {
  bool external;
  intr_handler_func* handler;

  /* External interrupts are special.
     We only handle one at a time (so interrupts must be off)
     and they need to be acknowledged on the PLIC (see below).
     An external interrupt handler cannot sleep. */
  external = frame->cause < 0 
            && (frame->cause & 0xf) >=8 && (frame->cause & 0xf) <= 11;
  
  if (external) {
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(!intr_context());

    in_external_intr = true;
    yield_on_return = false;
  }

  long vec_no = cause_to_vec_no(frame->cause);

  /* Invoke the interrupt's handler. */
  handler = intr_handlers[vec_no];
  if (handler != NULL)
    handler(frame);
  else
    unexpected_interrupt(frame);

  /* Complete the processing of an external interrupt. */
  if (external) {
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(intr_context());

    in_external_intr = false;
    plic_end_of_interrupt(frame->vec_no);

    if (yield_on_return)
      thread_yield();
  }
}

#if __riscv_xlen == 32
#define PRIx PRIx32
#else
#define PRIx PRIx64
#endif /* __riscv_xlen */

/* Handles an unexpected interrupt with interrupt frame F.  An
   unexpected interrupt is one that has no registered handler. */
static void unexpected_interrupt(const struct intr_frame* f) {
  long vec_no = cause_to_vec_no(f->cause);

  /* Count the number so far. */
  unsigned int n = ++unexpected_cnt[vec_no];

  /* If the number is a power of 2, print a message.  This rate
     limiting means that we get information about an uncommon
     unexpected interrupt the first time and fairly often after
     that, but one that occurs many times will not overwhelm the
     console. */
  if ((n & (n - 1)) == 0) {
    printf("Unexpected interrupt %#04x (%s), cause %08" PRIx "\n", 
          vec_no, intr_names[vec_no], f->cause);
  }
}

/* Dumps interrupt frame F to the console, for debugging. */
void intr_dump_frame(const struct intr_frame* f) {
  long vec_no = cause_to_vec_no(f->cause);

  /* stval is the linear address of the last page fault.
     See [riscv-priviledged-20211203] 
     4.1.9 "Supervisor Trap Value (stval) Register". */

  printf("Interrupt %#04x (%s) at epc=%p, cause %" PRIx "\n", vec_no, intr_names[vec_no], f->epc, f->cause);
  printf(" stval=%08" PRIx "\n", f->trap_val);
  printf(" ra=%08" PRIx " sp=%08" PRIx " gp=%08" PRIx " tp=%08" PRIx "\n", f->ra,
         f->sp, f->gp, f->tp);
  printf(" t0=%08" PRIx " t1=%08" PRIx " t2=%08" PRIx "\n", f->t0,
         f->t1, f->t2);
  printf(" s0=%08" PRIx " s1=%08" PRIx "\n", f->s0,
         f->s1);
  printf(" a0=%08" PRIx " a1=%08" PRIx " a2=%08" PRIx " a3=%08" PRIx "\n", f->a0,
         f->a1, f->a2, f->a3);
  printf(" a4=%08" PRIx " a5=%08" PRIx " a6=%08" PRIx " a7=%08" PRIx "\n", f->a4,
         f->a5, f->a6, f->a7);
  printf(" s2=%08" PRIx " s3=%08" PRIx " s4=%08" PRIx " s5=%08" PRIx "\n", f->s2,
         f->s3, f->s4, f->s5);
  printf(" s6=%08" PRIx " s7=%08" PRIx " s8=%08" PRIx " s9=%08" PRIx "\n", f->s6,
         f->s7, f->s8, f->s9);
  printf(" s10=%08" PRIx " s11=%08" PRIx "\n", f->s10,
         f->s11);
  printf(" t3=%08" PRIx " t4=%08" PRIx " t5=%08" PRIx " t6=%08" PRIx "\n", f->t3,
         f->t4, f->t5, f->t6);
}

/* Returns the name of interrupt VEC. */
const char* intr_name(uint8_t vec) { return intr_names[vec]; }
