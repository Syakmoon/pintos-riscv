#ifndef THREADS_LOADER_H
#define THREADS_LOADER_H

/* Physical address of kernel base. */
#define LOADER_KERN_BASE 0x20000 /* 128 kB. */

/* Kernel virtual address at which all physical memory is mapped.
   Must be aligned on a 4 MB boundary. */
#define LOADER_PHYS_BASE 0xc0000000 /* 3 GB. */

/* Kernel physical address base. Fixed by QEMU. */
#define KERNEL_PHYS_BASE 0x80000000 /* 2 GB. */

/* The amount of physical ram we provide to Pintos.
   4MB is chosen to pass the multi-oom tests. */
#define MEM_LENGTH 0x400000  /* 4MB. */

/* For booting arguments. */
#define LOADER_END KERNEL_PHYS_BASE + LOADER_KERN_BASE  /* Physical address of end of loader. */

/* Important loader physical addresses. */
#define LOADER_SIG (LOADER_END - LOADER_SIG_LEN)          /* 0xaa55 BIOS signature. */
#define LOADER_PARTS (LOADER_SIG - LOADER_PARTS_LEN)      /* Partition table. */
#define LOADER_ARGS (LOADER_PARTS - LOADER_ARGS_LEN)      /* Command-line args. */
#define LOADER_ARG_CNT (LOADER_ARGS - LOADER_ARG_CNT_LEN) /* Number of args. */

/* Sizes of loader data structures. */
#define LOADER_SIG_LEN 2
#define LOADER_PARTS_LEN 64
#define LOADER_ARGS_LEN 128
#define LOADER_ARG_CNT_LEN 4

/* GDT selectors defined by loader.
   More selectors are defined by userprog/gdt.h. */
#define SEL_NULL 0x00  /* Null selector. */
#define SEL_KCSEG 0x08 /* Kernel code selector. */
#define SEL_KDSEG 0x10 /* Kernel data selector. */

#ifndef __ASSEMBLER__
#include <stdint.h>

/* Amount of physical memory, in 4 kB pages. */
extern uintptr_t init_ram_pages;
#endif

#endif /* threads/loader.h */
