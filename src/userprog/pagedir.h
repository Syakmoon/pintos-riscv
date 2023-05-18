#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

#define MMIO_START 0xf0000000L

uint_t* pagedir_create(void);
void pagedir_destroy(uint_t* pd);
bool pagedir_set_page(uint_t* pd, void* upage, void* kpage, uint_t rwx);
void* pagedir_get_page(uint_t* pd, const void* upage);
void pagedir_clear_page(uint_t* pd, void* upage);
bool pagedir_is_dirty(uint_t* pd, const void* upage);
void pagedir_set_dirty(uint_t* pd, const void* upage, bool dirty);
bool pagedir_is_accessed(uint_t* pd, const void* upage);
void pagedir_set_accessed(uint_t* pd, const void* upage, bool accessed);
void pagedir_activate(uint_t* pd);
uint_t* active_pd(void);

#ifdef MACHINE
static void* pagedir_set_mmio(uint_t* pd, void* base, size_t size, bool writable) {
  return base;
}
#else
void* pagedir_set_mmio(uint_t* pd, void* base, size_t size, bool writable);
#endif

#endif /* userprog/pagedir.h */
