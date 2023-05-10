#ifndef THREADS_PTE_H
#define THREADS_PTE_H

#include "threads/vaddr.h"

/* Functions and macros for working with x86 hardware page
   tables.

   See vaddr.h for more generic functions and macros for virtual
   addresses.

   Virtual addresses are structured as follows:

    31                  22 21                  12 11                   0
   +----------------------+----------------------+----------------------+
   | Page Directory Index |   Page Table Index   |    Page Offset       |
   +----------------------+----------------------+----------------------+
*/

/* Page table index (bits 12:21). */
#define PTSHIFT PGBITS                  /* First page table bit. */
#define PTBITS 10                       /* Number of page table bits. */
#define PTSPAN (1 << PTBITS << PGBITS)  /* Bytes covered by a page table. */
#define PTMASK BITMASK(PTSHIFT, PTBITS) /* Page table bits (12:21). */

/* Page directory index (bits 22:31). */
#define PDSHIFT (PTSHIFT + PTBITS)      /* First page directory bit. */
#define PDBITS 10                       /* Number of page dir bits. */
#define PDMASK BITMASK(PDSHIFT, PDBITS) /* Page directory bits (22:31). */

/* Obtains page table index from a virtual address. */
static inline unsigned pt_no(const void* va) { return ((uintptr_t)va & PTMASK) >> PTSHIFT; }

/* Obtains page directory index from a virtual address. */
static inline uintptr_t pd_no(const void* va) { return (uintptr_t)va >> PDSHIFT; }

/* Page directory and page table entries.

   For more information see the section on page tables in the
   Pintos reference guide chapter, or [riscv-priviledged-20211203] 4.3
   "Sv32: Page-Based 32-bit Virtual-Memory Systems".

   PDEs and PTEs share a common format:

   31                                 10  9                     0
   +-------------------------------------+------------------------+
   |      PPN[1]      |      PPN[0]      |         Flags          |
   +-------------------------------------+------------------------+

   In a PDE, the physical address points to a page table.
   In a PTE, the physical address points to a data or code page.
   The important flags are listed below.
   When a PDE or PTE is not "valid", the other flags are
   ignored.
   A PDE or PTE that is initialized to 0 will be interpreted as
   "not present", which is just fine. */
#define PTE_FLAG_BITS 10     /* Number of flag bits. */
#define PTE_PGLEVEL_BITS 10  /* Number of bits per level. */
#define PTE_FLAGS 0x000003ff /* Flag bits. */
#define PTE_ADDR 0xfffffc00  /* Address bits. */
#define PTE_AVL 0x00000300   /* Bits available for OS use. */
#define PTE_V 0x1            /* 1=valid, 0=not valid. */
#define PTE_R 0x2            /* 1=readable, 0=not readable. */
#define PTE_W 0x4            /* 1=writable, 0=not writable. */
#define PTE_X 0x9            /* 1=executable, 0=not executable. */
#define PTE_U 0x10           /* 1=user/kernel, 0=kernel only. */
#define PTE_A 0x40           /* 1=accessed, 0=not acccessed. */
#define PTE_D 0x80           /* 1=dirty, 0=not dirty. */

/* Returns a PDE that points to page table PT. */
static inline uint_t pde_create(uint_t* pt) {
  ASSERT(pg_ofs(pt) == 0);
  /* For non-leaf PTEs, the D, A, and U bits are reserved
     for future standard use.  Until their use is defined by a
     standard extension, they must be cleared by software for
     forward compatibility. */
  return (pg_no((void*) vtop(pt)) << PTE_FLAG_BITS) | PTE_V;
}

/* Returns a pointer to the page table that page directory entry
   PDE, which must "valid", points to. */
static inline uint_t* pde_get_pt(uint_t pde) {
  ASSERT(pde & PTE_V);
  return ptov((pde & PTE_ADDR) >> PTE_FLAG_BITS << PGBITS);
}

/* Returns a PTE that points to PAGE.
   RWX will be included as well. */
static inline uint_t pte_create_general(void* page, uint_t rwx) {
  ASSERT(pg_ofs(page) == 0);
  return (pg_no((void*) page) << PTE_FLAG_BITS) | PTE_V | rwx;
}

/* Returns a PTE that points to PAGE.
   RWX will be included as well.
   The page will be usable only by Supervisor code (the kernel). */
static inline uint_t pte_create_kernel(void* page, uint_t rwx) {
  ASSERT(pg_ofs(page) == 0);
  return (pg_no((void*) vtop(page)) << PTE_FLAG_BITS) | PTE_V | rwx;
}

/* Returns a PTE that points to PAGE.
   RWX will be included as well.
   The page will be usable by both user and kernel code. */
static inline uint_t pte_create_user(void* page, uint_t rwx) {
  return pte_create_kernel(page, rwx) | PTE_U;
}

/* Returns a pointer to the page that page table entry PTE points
   to. */
static inline void* pte_get_page(uint_t pte) {
   return ptov((pte & PTE_ADDR) >> PTE_FLAG_BITS << PGBITS);
}

#endif /* threads/pte.h */
