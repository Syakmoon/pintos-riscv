	#include "threads/loader.h"
	#include "threads/vaddr.h"

#### Kernel startup code.

.section .init, "ax"
.global start
start:
    .cfi_startproc
    .cfi_undefined ra
	# Sets up the first stack to be at the top of 
	# the next higher page after __kernel_end
    la sp, __kernel_end
    li t0, PGMASK
    li t1, 2*PGSIZE-1
	add sp, sp, t1
	not t0, t0
	and sp, sp, t0
    add s0, sp, zero

	# Reads the amount of physically memory size
	la t0, __mem_length
	srli t0, t0, 12	# 2^12 = 4kB
	la t1, init_ram_pages
	sw	t0, 0(t1)# TODO: change sw to MACRO'd
	
    jal zero, kmain
    .cfi_endproc

#### Physical memory size in 4 kB pages.  This is exported to the rest
#### of the kernel.
.globl init_ram_pages
init_ram_pages:
	.long 0
.end
