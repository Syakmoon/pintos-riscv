#include "threads/loader.h"

OUTPUT_ARCH(riscv)
ENTRY(main)			/* Kernel starts at "main" symbol. */
SECTIONS
{
  /* Specify the kernel base address. */
  _start = LOADER_PHYS_BASE + LOADER_KERN_BASE;

  . = _start + SIZEOF_HEADERS;

  /* Kernel starts with code, followed by read-only data and writable data. */
  .text : { *(.start) *(.text) } = 0x13
  .rodata : { *(.rodata) *(.rodata.*)
	      . = ALIGN(0x1000);
	      _end_kernel_text = .; }
  .eh_frame : { *(.eh_frame) }
  .data : { *(.data)
	    _signature = .; LONG(0xaa55aa55) }

  .plt : { *(.plt*) }

  /* BSS (zero-initialized data) is after everything else. */
  _start_bss = .;
  .bss : { *(.bss) }
  _end_bss = .;

  _end = .;

  ASSERT (_end - _start <= 512K, "Kernel image is too big.")
}
