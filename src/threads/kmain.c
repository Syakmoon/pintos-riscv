#include <riscv.h>
#include <stdint.h>
#include <debug.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "devices/timer.h"
#include "devices/virtio-blk.h"

extern void mintr_entry();
// extern int main(void);

void* fdt_ptr;
uintptr_t next_avail_address;

int main(void) {
  thread_init();
  console_init();

  /* Initialize memory system. */
  palloc_init(SIZE_MAX);
  malloc_init();
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

/* Clear the "BSS", a segment that should be initialized to
   zeros.

   The start and end of the BSS segment is recorded by the
   linker as _start_bss and _end_bss.  See kernel.lds. */
static void bss_init(void) {
  extern char _start_bss, _end_bss;
  memset(&_start_bss, 0, &_end_bss - &_start_bss);
}

/* Initializes the mstatus register. */
static void mstatus_init() {
    uintptr_t mstatus = csr_read(CSR_MSTATUS);

    /* Enable SUM to allow accessing user memory in Supervisor. */
    mstatus |= MSTATUS_SUM;

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

/* Populates the base page table and page table with the
   kernel virtual mapping, and then sets up the CPU to use the
   new page table.  Points init_page_dir to the page
   table it creates.
   This is to ensure that when it enters Supervisor,
   every kernel address is >= PHYS_BASE */
static void init_paging(void) {
  uint_t *pd, *pt;
  size_t page;
  size_t megapage_increment = 1 << PTE_PGLEVEL_BITS;

  /* Gets the next page to use. */
  asm volatile ("mv %0, sp" : "=r" (next_avail_address) : : "memory");
  next_avail_address = pg_round_up(next_avail_address) + PGSIZE;

  pd = init_page_dir = __M_mode_alloc(&next_avail_address, 1);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page+=megapage_increment) {
    uintptr_t paddr = page * PGSIZE + KERN_BASE;
    char* vaddr = ptov(paddr);
    size_t pde_idx = pd_no(vaddr);

    // TEMP: do not do this after loader is full-fledged
    size_t pde_idx_identical = pd_no(paddr);
    pd[pde_idx] = (pg_no(paddr) << PTE_FLAG_BITS) | PTE_R | PTE_W | PTE_X | PTE_V;
    pd[pde_idx_identical] = pd[pde_idx];
  }

  /* Store the physical address of the page directory into SATP.
     This activates our new page tables immediately.
     See [riscv-priviledged-20211203] 4.1.11 "Supervisor Address Translation
     and Protection (satp) Register". */
  csr_write(CSR_SATP, (1<<31) | pg_no(init_page_dir));
  sfence_vma();

  // TEMP: do not do this after loader is full-fledged
  /* Relocation. */
  init_page_dir = ptov(init_page_dir);
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

    machine_interrupt_init();

    uintptr_t init_stack = ptov(next_avail_address + PGSIZE);
    uintptr_t converted_fdt_ptr = ptov(fdt_ptr);
    uintptr_t init_main = ptov(main);

    // TEMP: we pretend that timer interrupt won't not happen before mret
    /* Set up the init thread's stack. */
    asm volatile("mv sp, %0" : : "r" (init_stack): "memory");

    /* Pass in the pointer to fdt as the first argument */
    asm volatile("mv a0, %0" : : "r" (converted_fdt_ptr): "memory");

    /* Null-terminate main()'s backtrace. */
    asm volatile("mv ra, %0" : : "r" (0));

    /* Set MEPC to main to mret to this function. */
    csr_write(CSR_MEPC, init_main);

    mret();
    __builtin_unreachable();    /* Forbids returning. */
}

/* start.S will jump to this function.
   By default, QEMU sets A0 as hart ID, and A1 as a pointer to FDT. */
void kmain(int hart UNUSED, void* fdt) {
    /* Clear BSS. */
    bss_init();

    /* We save the pointer to fdt for main to discover device information. */
    fdt_ptr = fdt;

    mstatus_init();
    // fp_init();
    delegate_traps();
    virtio_blks_init(POLL);
    pmp_init();
    init_paging();
    timer_init_machine();

    /* Switch to Supervisor mode. */
    return_to_supervisor();

    /* It should never return to this function. */
    NOT_REACHED();
}
