	#include "threads/loader.h"
	#include "threads/vaddr.h"

#### Kernel startup code.

.section .init, "ax"
.global start
start:
    .cfi_startproc
    .cfi_undefined ra
	# Sets up the first stack to be at the top of 
	# the next higher page after _end
    la sp, _end
    li t0, PGMASK
    li t1, 2*PGSIZE-1
	add sp, sp, t1
	not t0, t0
	and sp, sp, t0
    add s0, sp, zero
	
    call zero, kmain
    .cfi_endproc
.end
