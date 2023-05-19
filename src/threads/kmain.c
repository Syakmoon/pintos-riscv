#include <riscv.h>
#include <stdint.h>
#include <debug.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "devices/timer.h"
#include "devices/virtio-blk.h"
#include "devices/block.h"
#include "devices/serial.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"

extern void intr_entry();

void* fdt_ptr;
size_t init_ram_pages;
uintptr_t next_avail_address;
uintptr_t kernel_position;

/* Different from init.c's copy. */
uintptr_t* init_page_dir;

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

  init_ram_pages = MEM_LENGTH >> PGBITS;

  /* Gets the next page to use. */
  asm volatile ("mv %0, sp" : "=r" (next_avail_address) : : "memory");
  next_avail_address = pg_round_up(next_avail_address) + PGSIZE;

  pd = init_page_dir = __M_mode_palloc(&next_avail_address, 1);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page+=megapage_increment) {
    uintptr_t paddr = page * PGSIZE + KERN_BASE;
    char* vaddr = ptov(paddr);
    size_t pde_idx = pd_no(vaddr);

    pd[pde_idx] = (pg_no(paddr) << PTE_FLAG_BITS) | PTE_R | PTE_W | PTE_X | PTE_V;
  }

  /* Sets up a mapped page for serial and shutdown in the MMIO region. */
  pt = __M_mode_palloc(&next_avail_address, 1);
  pt[0] = (pg_no(SERIAL_MMIO_BASE) << PTE_FLAG_BITS) | PTE_R | PTE_W | PTE_V;
  pt[1] = (pg_no(QEMU_TEST_MMIO) << PTE_FLAG_BITS) | PTE_R | PTE_W | PTE_V;
  pd[pd_no(MMIO_START)] = (pg_no(pt) << PTE_FLAG_BITS) | PTE_V;

  /* Stores the physical address of the page directory into SATP.
     This activates our new page tables immediately.
     See [riscv-priviledged-20211203] 4.1.11 "Supervisor Address Translation
     and Protection (satp) Register". */
  csr_write(CSR_SATP, (1<<31) | pg_no(init_page_dir));
  sfence_vma();
}

static void load_supervisor_kernel(void) {
  struct block* device;
  uintptr_t position;

  virtio_blks_init(POLL);

  /* NEXT_AVAIL_ADDRESS is capped at 2 pages under Supervisor kernel's base.
     One page at the lower address for initial thread's stack.  This is to
     keep consistent with x86 Pintos.  From x86 Pintos loader.S:

     Set stack to grow downward from 60 kB (after boot, the kernel
     continues to use this stack for its initial thread).
     
     The next page at the higher address uses its last block space (512 Bytes)
     for the "loader", which is now a zero'd space only containing arguments
     written by the Pintos program. */
  ASSERT(next_avail_address <=
        KERNEL_PHYS_BASE + LOADER_KERN_BASE - 2 * ((uintptr_t) PGSIZE));
  kernel_position = KERNEL_PHYS_BASE + LOADER_KERN_BASE;

  /* To support reading arguments passed in by the Pintos program. */
  device = block_get_by_name("hda");
  block_read(device, 0, kernel_position - BLOCK_SECTOR_SIZE);

  while(block_type(device) != BLOCK_KERNEL) {
    device = block_next(device);
    ASSERT(device != NULL);
  }

  /* Load the kernel ELF file. */
  for (block_sector_t i = 0; i < block_size(device); ++i) {
    block_read(device, i, kernel_position);
    kernel_position += BLOCK_SECTOR_SIZE;
  }
}

/* Sets up the Machine trap handler and enables interrupts. */
static void machine_interrupt_init() {
  uintptr_t mstatus = csr_read(CSR_MSTATUS);

  /* Enable Machine interrupts. */
  mstatus |= MSTATUS_MIE;

  csr_write(CSR_MTVEC, intr_entry);
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

  uintptr_t init_stack = ptov(KERNEL_PHYS_BASE + LOADER_KERN_BASE -
                        (uintptr_t) PGSIZE); /* 60 kB */
  uintptr_t converted_fdt_ptr = ptov(fdt_ptr);
  uintptr_t init_main = KERNEL_PHYS_BASE + LOADER_KERN_BASE + 0x18;
  init_main = *(uintptr_t*) init_main;

  // TEMP: we pretend that timer interrupt won't not happen before mret
  /* Set up the init thread's stack. */
  asm volatile("mv sp, %0" : : "r" (init_stack): "memory");

  /* Pass in the pointer to fdt as the first argument. */
  asm volatile("mv a0, %0" : : "r" (converted_fdt_ptr): "memory");

  /* Pass in the number of ram pages in the second argument. */
  asm volatile("mv a1, %0" : : "r" (init_ram_pages): "memory");

  /* Null-terminate main()'s backtrace. */
  asm volatile("mv ra, %0" : : "r" (NULL));

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
  delegate_traps();
  pmp_init();
  init_paging();
  init_poll();
  load_supervisor_kernel();
  timer_init_machine();

  /* Switch to Supervisor mode. */
  return_to_supervisor();

  /* It should never return to this function. */
  NOT_REACHED();
}
