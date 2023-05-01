	#include "threads/loader.h"

#### Kernel startup code.

.section .init, "ax"
.global start
start:
    .cfi_startproc
    .cfi_undefined ra
    la sp, __stack_top
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
