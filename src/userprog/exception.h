#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1 /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2 /* 0: read, 1: write. */
#define PF_U 0x4 /* 0: kernel, 1: user process. */

/* Exceptions. */
#define EXC_INSTRUCTION_MISALIGNED 0
#define EXC_INSTRUCTION_FAULT 1
#define EXC_ILLEGAL_INSTRUCTION 2
#define EXC_BREAKPOINT 3
#define EXC_LOAD_MISALIGNED 4
#define EXC_LOAD_FAULT 5
#define EXC_STORE_MISALIGNED 6
#define EXC_STORE_FAULT 7
#define EXC_ECALL_U 8
#define EXC_ECALL_S 9
#define EXC_ECALL_M 11
#define EXC_INSTRUCTION_PAGE_FAULT 12
#define EXC_LOAD_PAGE_FAULT 13
#define EXC_STORE_PAGE_FAULT 15

void exception_init(void);
void exception_print_stats(void);

#endif /* userprog/exception.h */
