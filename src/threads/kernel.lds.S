#include "threads/loader.h"

OUTPUT_ARCH(riscv)
ENTRY(main)			/* Kernel starts at "main" symbol. */
SECTIONS
{
  MEM_LENGTH = 0x4000000;
  PROVIDE(__mem_length = MEM_LENGTH);
  /* Specify the kernel base address. */
  _start = LOADER_PHYS_BASE + 0x30000;

  . = _start;

  /* Kernel starts with code, followed by read-only data and writable data. */
  .init : {
        KEEP (*(SORT_NONE(.init)))
  }
  .text : { *(.start) 
        *(.text) }
  .rodata : { 
        . = ALIGN(0x1000);
        *(.rodata) *(.rodata.*)
        . = ALIGN(0x1000);
	      _end_kernel_text = .; }
  .eh_frame : { *(.eh_frame) }
  .data : { 
	      . = ALIGN(0x1000);
        *(.data)
	    _signature = .; LONG(0xaa55aa55) }

  .plt : { 
	      . = ALIGN(0x1000);
        *(.plt*) }

  /* BSS (zero-initialized data) is after everything else. */
  . = ALIGN(0x1000);
  _start_bss = .;
  .bss : { 
        *(.bss) }
  _end_bss = .;

  _end = .;

  PROVIDE(__kernel_start = _start);
  PROVIDE(__kernel_end = _end);

  ASSERT (_end - _start <= 512K, "Kernel image is too big.")
}
