#include "threads/loader.h"

OUTPUT_ARCH(riscv)
ENTRY(start)			/* Loader starts at "start" symbol. */
SECTIONS
{
  /* Specify the kernel base address. */
  _start = KERNEL_PHYS_BASE;

  . = _start;

  /* Kernel starts with code, followed by read-only data and writable data. */
  .init : {
        KEEP (*(SORT_NONE(.init)))
  }
  .text : { *(.start) *(.text) } = 0x13
  .rodata : { *(.rodata) *(.rodata.*)
	      . = ALIGN(0x1000); }
  .eh_frame : { *(.eh_frame) }
  .data : { *(.data) }

  .plt : { *(.plt*) }

  /* BSS (zero-initialized data) is after everything else. */
  _start_bss = .;
  .bss : { *(.bss) }
  _end_bss = .;

  _end = .;

  ASSERT (_end - _start <= 64K, "Loader image is too big.")
}
