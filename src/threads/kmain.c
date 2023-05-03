#include <riscv.h>
#include <stdint.h>
// #include "threads/init.h"
#include "devices/timer.h"

extern void mintr_entry();
extern int main(void);

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
    csr_write(CSR_PMPADDR0, -1UL);
    uintptr_t pmpcfg = PMP_CFG_R | PMP_CFG_W | PMP_CFG_X;
    csr_write(CSR_PMPCFG0, pmpcfg);
}

/* Sets up the Machine trap handler and enables interrupts. */
static void machine_interrupt_init() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);
    uintptr_t mie = csr_read(CSR_MIE);

    /* Enable Machine interrupts. */
    mstatus |= MSTATUS_MIE;
    
    /* Enable Machine timer interrupt. */
    mie |= INT_MTI;

    csr_write(CSR_MTVEC, mintr_entry);
    csr_write(CSR_MSTATUS, mstatus);
    csr_write(CSR_MIE, mie);
}

static void return_to_supervisor() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);

    /* Set MPP to Supervisor. */
    mstatus &= ~MSTATUS_MPP;
    mstatus |= MSTATUS_MPP_S;

    csr_write(CSR_MSTATUS, mstatus);

    /* Set MEPC to main to mret to this function. */
    // csr_write(CSR_MEPC, main);

    machine_interrupt_init();

    mret();
}

/* start.S will jump to this function. */
void kmain() {
    mstatus_init();

    /* Disable paging. */
    csr_write(CSR_SATP, 0);

    // fp_init();
    delegate_traps();
    pmp_init();
    timer_init();

    /* Switch to Supervisor mode. */
    return_to_supervisor();

    /* It should never return to this function.
       We shall not call NOT_REACHED() because UART is not set up yet. */
    __builtin_unreachable();
    wfi();
}
