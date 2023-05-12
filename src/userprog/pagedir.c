#include "userprog/pagedir.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <riscv.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"

static void invalidate_pagedir(uint_t*);

#define SATP32_MODE 0x80000000
#define SATP32_PPN  0x003FFFFF
#define SATP64_MODE 0xF000000000000000
#define SATP64_PPN  0x00000FFFFFFFFFFF

#define SATP_MODE_SV32 1 << 31
#define SATP_MODE_SV39 8 << 60

#if __riscv_xlen == 32
#define SATP_MODE SATP32_MODE
#define SATP_PPN SATP32_PPN
#define SATP_SV SATP_MODE_SV32
#else
#define SATP_MODE SATP64_MODE
#define SATP_PPN SATP64_PPN
#define SATP_SV SATP_MODE_SV39
#endif /* __riscv_xlen */

/* After PHYS_BASE, we set 0xf0000000 as the base for MMIO.
   Because we have allocated the first two pages for serial and shutdown,
   we set it to 0xf0002000. */
uintptr_t mmio_next_available = MMIO_START + 2 * PGSIZE;

/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint_t* pagedir_create(void) {
  uint_t* pd = palloc_get_page(0);
  if (pd != NULL)
    memcpy(pd, init_page_dir, PGSIZE);
  return pd;
}

/* Destroys page directory PD, freeing all the pages it
   references. */
void pagedir_destroy(uint_t* pd) {
  uint_t* pde;

  if (pd == NULL)
    return;

  ASSERT(pd != init_page_dir);
  for (pde = pd; pde < pd + pd_no(PHYS_BASE); pde++)
    if (*pde & PTE_V) {
      uint_t* pt = pde_get_pt(*pde);
      uint_t* pte;

      for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
        if (*pte & PTE_V)
          palloc_free_page(pte_get_page(*pte));
      palloc_free_page(pt);
    }
  palloc_free_page(pd);
}

/* If GENERAL is false, it also checks if it fits the Pintos kernel-user
   memory layout. */
static uint_t* __lookup_page(uint_t* pd, const void* vaddr, bool create, bool general) {
  uint_t *pt, *pde;

  ASSERT(pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
  if (!general)
    ASSERT(!create || is_user_vaddr(vaddr));

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no(vaddr);
  if (*pde == 0) {
    if (create) {
      pt = palloc_get_page(PAL_ZERO);
      if (pt == NULL)
        return NULL;

      *pde = pde_create(pt);
    } else
      return NULL;
  }

  /* Return the page table entry. */
  pt = pde_get_pt(*pde);
  return &pt[pt_no(vaddr)];
}

/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint_t* lookup_page(uint_t* pd, const void* vaddr, bool create) {
  return __lookup_page(pd, vaddr, create, false);
}

/* If GENERAL is false, it also checks if it fits the Pintos kernel-user
   memory layout. */
bool __pagedir_set_page(uint_t* pd, void* vaddr, void* paddr, uint_t rwx, bool general) {
  uint_t* pte;

  ASSERT(pg_ofs(vaddr) == 0);
  ASSERT(pg_ofs(paddr) == 0);
  if (!general) {
    ASSERT(is_user_vaddr(vaddr));
    ASSERT(vtop(paddr) >> PTSHIFT < init_ram_pages);
    ASSERT(pd != init_page_dir);
  }

  pte = __lookup_page(pd, vaddr, true, general);

  if (pte != NULL) {
    ASSERT((*pte & PTE_V) == 0);
    if (!general)
      *pte = pte_create_user(paddr, rwx);
    else
      *pte = pte_create_general(paddr, rwx);
    sfence_vma();
    return true;
  } else
    return false;
}

/* Adds a mapping in page directory PD from user virtual page
   VADDR to the physical frame identified by kernel virtual
   address KPAGE.
   VADDR must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   RWX will be included as well.
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool pagedir_set_page(uint_t* pd, void* upage, void* kpage, uint_t rwx) {
  return __pagedir_set_page(pd, upage, kpage, rwx, false);
}

/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void* pagedir_get_page(uint_t* pd, const void* uaddr) {
  uint_t* pte;

  ASSERT(is_user_vaddr(uaddr));

  pte = lookup_page(pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_V) != 0)
    return pte_get_page(*pte) + pg_ofs(uaddr);
  else
    return NULL;
}

/* Maps BASE~BASE+SIZE to mmio_next_available~mmio_next_available+BASE,
   then returns the bottom of that region.
   WARNING: After the first userprog is set up, DO NOT call this function,
   or the MMIO region becomes inconsistent among page tables. */
void* pagedir_set_mmio(uint_t* pd, void* base, size_t size, bool writable) {
  void* old = mmio_next_available;
  size = pg_round_up(size);
  while (mmio_next_available < ((uintptr_t) old) + size) {
    uint_t rwx = writable ? PTE_R | PTE_W : PTE_R;
    ASSERT(__pagedir_set_page(pd, mmio_next_available, (uintptr_t) base, rwx, true));
    base += PGSIZE;
    mmio_next_available += PGSIZE;
    ASSERT(mmio_next_available > 0xf0000000);
  }
  return old;
}

/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void pagedir_clear_page(uint_t* pd, void* upage) {
  uint_t* pte;

  ASSERT(pg_ofs(upage) == 0);
  ASSERT(is_user_vaddr(upage));

  pte = lookup_page(pd, upage, false);
  if (pte != NULL && (*pte & PTE_V) != 0) {
    *pte &= ~PTE_V;
    invalidate_pagedir(pd);
  }
}

/* Returns true if the PTE for virtual page VPAGE in PD is dirty,
   that is, if the page has been modified since the PTE was
   installed.
   Returns false if PD contains no PTE for VPAGE. */
bool pagedir_is_dirty(uint_t* pd, const void* vpage) {
  uint_t* pte = lookup_page(pd, vpage, false);
  return pte != NULL && (*pte & PTE_D) != 0;
}

/* Set the dirty bit to DIRTY in the PTE for virtual page VPAGE
   in PD. */
void pagedir_set_dirty(uint_t* pd, const void* vpage, bool dirty) {
  uint_t* pte = lookup_page(pd, vpage, false);
  if (pte != NULL) {
    if (dirty)
      *pte |= PTE_D;
    else {
      *pte &= ~(uint_t)PTE_D;
      invalidate_pagedir(pd);
    }
  }
}

/* Returns true if the PTE for virtual page VPAGE in PD has been
   accessed recently, that is, between the time the PTE was
   installed and the last time it was cleared.  Returns false if
   PD contains no PTE for VPAGE. */
bool pagedir_is_accessed(uint_t* pd, const void* vpage) {
  uint_t* pte = lookup_page(pd, vpage, false);
  return pte != NULL && (*pte & PTE_A) != 0;
}

/* Sets the accessed bit to ACCESSED in the PTE for virtual page
   VPAGE in PD. */
void pagedir_set_accessed(uint_t* pd, const void* vpage, bool accessed) {
  uint_t* pte = lookup_page(pd, vpage, false);
  if (pte != NULL) {
    if (accessed)
      *pte |= PTE_A;
    else {
      *pte &= ~(uint_t)PTE_A;
      invalidate_pagedir(pd);
    }
  }
}

/* Loads page directory PD into the CPU's page directory base
   register. */
void pagedir_activate(uint_t* pd) {
  if (pd == NULL)
    pd = init_page_dir;

  /* The SFENCE.VMA is used to flush any local hardware caches related to 
     address translation.  It is specified as a fence rather than a TLB flush
     to provide cleaner semantics with respect to which instructions are 
     affected by the flush operation and to support a wider variety of dynamic
     caching structures and memory-management schemes.  SFENCE.VMA is also
     used by higher privilege levels to synchronize page table writes and 
     the address translation hardware. */
  sfence_vma();

  /* Store the physical address of the page directory into SATP.  
     This activates our new page tables immediately.
     See [riscv-priviledged-20211203] 4.1.11 "Supervisor Address Translation
     and Protection (satp) Register". */
  csr_write(CSR_SATP, (pg_no((vtop(pd))) & SATP32_PPN) | SATP_SV);

  /* Make sure we the new page table takes into effect. */
  sfence_vma();
}

/* Returns the currently active page directory. */
uint_t* active_pd(void) {
  /* Copy SATP, extract the PPN, into `pd'.
     If SV32 if used, the highest two bits are discarded.
     See [riscv-priviledged-20211203] 4.1.11 "Supervisor Address Translation
     and Protection (satp) Register". */
  uintptr_t pd;
  pd = (csr_read(CSR_SATP) & SATP32_PPN) << PGBITS;
  return ptov(pd);
}

/* Seom page table changes can cause the CPU's translation
   lookaside buffer (TLB) to become out-of-sync with the page
   table.  When this happens, we have to "invalidate" the TLB by
   re-activating it.

   This function invalidates the TLB if PD is the active page
   directory.  (If PD is not active then its entries are not in
   the TLB, so there is no need to invalidate anything.) */
static void invalidate_pagedir(uint_t* pd) {
  if (active_pd() == pd) {
    sfence_vma();
  }
}
