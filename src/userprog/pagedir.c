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

/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint_t* lookup_page(uint_t* pd, const void* vaddr, bool create) {
  uint_t *pt, *pde;

  ASSERT(pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
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

/* Adds a mapping in page directory PD from user virtual page
   UPAGE to the physical frame identified by kernel virtual
   address KPAGE.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   RWX will be included as well.
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool pagedir_set_page(uint_t* pd, void* upage, void* kpage, uint_t rwx) {
  uint_t* pte;

  ASSERT(pg_ofs(upage) == 0);
  ASSERT(pg_ofs(kpage) == 0);
  ASSERT(is_user_vaddr(upage));
  ASSERT(vtop(kpage) >> PTSHIFT < init_ram_pages);
  ASSERT(pd != init_page_dir);

  pte = lookup_page(pd, upage, true);

  if (pte != NULL) {
    ASSERT((*pte & PTE_V) == 0);
    *pte = pte_create_user(kpage, rwx);
    return true;
  } else
    return false;
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
