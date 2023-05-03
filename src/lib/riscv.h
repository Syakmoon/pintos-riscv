#ifndef __LIB_RISCV_H
#define __LIB_RISCV_H

#define UART 0x10000000L

#define CSR_MSTATUS		0x300
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE			0x304
#define CSR_MTVEC		0x305
#define CSR_MSCRATCH		0x340
#define CSR_MEPC		0x341
#define CSR_MCAUSE		0x342
#define CSR_MTVAL		0x343
#define CSR_PMPCFG0		0x3a0
#define CSR_PMPADDR0		0x3b0

#define CSR_SSTATUS		0x100
#define CSR_SIE			0x104
#define CSR_STVEC		0x105
#define CSR_SCOUNTEREN		0x106
#define CSR_SSCRATCH		0x140
#define CSR_SEPC		0x141
#define CSR_SCAUSE		0x142
#define CSR_STVAL		0x143
#define CSR_SIP			0x144
#define CSR_SATP		0x180

/* Register mstatus. */
#define MSTATUS_MPP_BIT     11
#define MSTATUS_MIE         0x00000008
#define MSTATUS_MPP         0x00001800
#define MSTATUS_MPP_S       0x00000800
#define MSTATUS_FS          0x00006000

// TODO: Put these into interrupt.h
/* Interrupts. */
#define IRQ_S_SOFTWARE      1
#define IRQ_S_TIMER         5
#define IRQ_M_TIMER         7
#define IRQ_S_EXTERNAL      9
#define IRQ_M_EXTERNAL      11

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

/* PMP configuration register. */
#define PMP_CFG_R           0x01
#define PMP_CFG_W           0x02
#define PMP_CFG_X           0x04

/* Converted IRQs. */
#define INT_SSI             (1 << IRQ_S_SOFTWARE)
#define INT_STI             (1 << IRQ_S_TIMER)
#define INT_MTI             (1 << IRQ_M_TIMER)
#define INT_SEI             (1 << IRQ_S_EXTERNAL)

#ifndef __ASSEMBLER__
#define __ASM_STR(x)	#x

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	asm volatile ("csrr %0, " __ASM_STR(csr)	\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	asm volatile ("csrw " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#define wfi()						\
({								\
	asm volatile ("wfi" : : : "memory");  \
})

#define mret()						\
({								\
	asm volatile ("mret");  \
})

#else
#if __riscv_xlen == 32
#define REG_S sw
#define REG_L lw
#define REGBYTES 4
#else
#define REG_S sd
#define REG_L ld
#define REGBYTES 8
#endif /* __riscv_xlen */

#endif /* __ASSEMBLER__ */

#endif /* lib/riscv.h */
