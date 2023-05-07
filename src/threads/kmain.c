#include <riscv.h>
#include <stdint.h>
#include <debug.h>
// #include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "devices/timer.h"

extern void mintr_entry();
// extern int main(void);

void* fdt_ptr;

int main(void) {
    intr_init();
    intr_enable();
    for(int a = 0; a < INTMAX_MAX; ++a){
        intr_disable();
        for(int b = 0; b < 500000;){
            ++b;
        }    // This should be for at least one timer interrupt
        csr_write(CSR_SSTATUS, csr_read(CSR_SSTATUS) | SSTATUS_SIE);
        wfi();
    }
    return 1024;
}

/* Initializes the mstatus register. */
static void mstatus_init() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);

    /* Enable FPU. */
    // mstatus |= MSTATUS_FS;

    csr_write(CSR_MSTATUS, mstatus);
}

/* Delegates all interrupts and exceptions to Supervisor. */
static void delegate_traps() {
    uintptr_t mideleg = INT_SSI | INT_STI | INT_SEI;

    /* All exceptions */
    uintptr_t medeleg = -1UL;

    csr_write(CSR_MIDELEG, mideleg);
    csr_write(CSR_MEDELEG, medeleg);
}

/* Sets up the pmp to allow Supervisor to acess all of physical memory. */
static void pmp_init() {
    /* If TOR is selected, the associated address register forms 
       the top of the address range, and the preceding PMP address 
       register forms the bottom of the address range.
       We used PMP entry 0 here, so it will match any address y < PMPADDR0. */
    csr_write(CSR_PMPADDR0, -1UL);
    uintptr_t pmpcfg = PMP_CFG_A_TOR | PMP_CFG_R | PMP_CFG_W | PMP_CFG_X;
    csr_write(CSR_PMPCFG0, pmpcfg);
}

/* Sets up the Machine trap handler and enables interrupts. */
static void machine_interrupt_init() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);

    /* Enable Machine interrupts. */
    mstatus |= MSTATUS_MIE;

    csr_write(CSR_MTVEC, mintr_entry);
    csr_write(CSR_MSTATUS, mstatus);

    asm volatile ("csrw mscratch, sp");
}

static void return_to_supervisor() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);

    /* Set MPP to Supervisor. */
    mstatus &= ~MSTATUS_MPP;
    mstatus |= MSTATUS_MPP_S;

    csr_write(CSR_MSTATUS, mstatus);

    /* Set MEPC to main to mret to this function. */
    csr_write(CSR_MEPC, main);

    machine_interrupt_init();

    // TEMP: we pretend that timer interrupt won't not happen before mret
    /* Set up the init thread's stack. */
    unsigned long sp_value;
    asm volatile ("mv %0, sp" : "=r" (sp_value) : : "memory");
    sp_value = pg_round_up(sp_value) + PGSIZE;
    asm volatile("mv sp, %0" : : "r" (sp_value): "memory");

    /* Null-terminate main()'s backtrace. */
    asm volatile("mv ra, %0" : : "r" (0));

    /* Pass in the pointer to fdt as the first argument */
    asm volatile("mv a0, %0" : : "r" (fdt_ptr): "memory");

    mret();
    __builtin_unreachable();    /* Forbids returning. */
}

/* start.S will jump to this function.
   By default, QEMU sets A0 as hart ID, and A1 as a pointer to FDT. */
void kmain(int hart UNUSED, void* fdt) {
    /* We save the pointer to fdt for main to discover device information. */
    fdt_ptr = fdt;

    mstatus_init();

    /* Disable paging. */
    csr_write(CSR_SATP, 0);

    // fp_init();
    delegate_traps();
    pmp_init();
    timer_init();

    /* Switch to Supervisor mode. */
    return_to_supervisor();

    /* It should never return to this function. */
    NOT_REACHED();
}
