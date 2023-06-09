#ifndef __LIB_RISCV_H
#define __LIB_RISCV_H

#define CSR_MSTATUS		0x300
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE			0x304
#define CSR_MTVEC		0x305
#define CSR_MSCRATCH		0x340
#define CSR_MEPC		0x341
#define CSR_MCAUSE		0x342
#define CSR_MTVAL		0x343
#define CSR_MIP 0x344
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
#define MSTATUS_SUM         0x00040000

/* Register sstatus. */
#define SSTATUS_SIE         0x00000002
#define SSTATUS_SPIE        0x00000020
#define SSTATUS_SPP         0x00000100
#define SSTATUS_SUM         0x00040000


#ifdef MACHINE
#define CSR_STATUS	CSR_MSTATUS
#define CSR_SCRATCH	CSR_MSCRATCH
#define CSR_EPC		CSR_MEPC
#define CSR_CAUSE	CSR_MCAUSE
#define CSR_TVAL	CSR_MTVAL
#define XRET		mret
#else
#define CSR_STATUS	CSR_SSTATUS
#define CSR_SCRATCH	CSR_SSCRATCH
#define CSR_EPC		CSR_SEPC
#define CSR_CAUSE	CSR_SCAUSE
#define CSR_TVAL	CSR_STVAL
#define XRET		sret
#endif

/* PMP configuration register. */
#define PMP_CFG_R           0x01
#define PMP_CFG_W           0x02
#define PMP_CFG_X           0x04
#define PMP_CFG_A_TOR		0x08

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
	asm volatile ("wfi");  \
})

#define mret()						\
({								\
	asm volatile ("mret");  \
})

#define sfence_vma()						\
({								\
	asm volatile("sfence.vma");  \
})
#endif /* __ASSEMBLER__ */

#if __riscv_xlen == 32
#define REG_S sw
#define REG_L lw
#define REGBYTES 4
#define TYP w
#else
#define REG_S sd
#define REG_L ld
#define REGBYTES 8
#define TYP d
#endif /* __riscv_xlen */
#define XSTR(s) STR(s)
#define STR(s) #s

#endif /* lib/riscv.h */
