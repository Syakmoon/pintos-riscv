#include "threads/loader.h"

OUTPUT_ARCH(riscv)
ENTRY(start)			/* Kernel starts at "start" symbol. */
SECTIONS
{
  MEM_LENGTH = 0x4000000;
  PROVIDE(__qemu_fdt = LOADER_PHYS_BASE + MEM_LENGTH - 0x200000);
  PROVIDE(__stack_top = __qemu_fdt);
  PROVIDE(__mem_length = MEM_LENGTH);
  /* Specify the kernel base address. */
  _start = LOADER_PHYS_BASE;

  . = _start;

  /* Kernel starts with code, followed by read-only data and writable data. */
  .init : {
        KEEP (*(SORT_NONE(.init)))
	      . = ALIGN(0x1000);
  }
  .text : { *(.start) 
	      . = ALIGN(0x1000);
        *(.text)
	      . = ALIGN(0x1000); }
  .rodata : { 
	      . = ALIGN(0x1000);
        *(.rodata) *(.rodata.*)
	      . = ALIGN(0x1000);
	      _end_kernel_text = .; }
  .eh_frame : { *(.eh_frame) }
  .data : { 
	      . = ALIGN(0x1000);
        *(.data)
	      . = ALIGN(0x1000);
	    _signature = .; LONG(0xaa55aa55) }

  .plt : { 
	      . = ALIGN(0x1000);
        *(.plt*) }

  /* BSS (zero-initialized data) is after everything else. */
  _start_bss = .;
  .bss : { 
	      . = ALIGN(0x1000);
        *(.bss) }
  _end_bss = .;

  _end = .;

  ASSERT (_end - _start <= 512K, "Kernel image is too big.")
}
