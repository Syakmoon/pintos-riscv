Pintos RISC-V
=============

[![build](https://github.com/Syakmoon/pintos-riscv/actions/workflows/makefile.yml/badge.svg)](https://github.com/Syakmoon/pintos-riscv/actions/workflows/makefile.yml)
[![do-nothing](https://github.com/Syakmoon/pintos-riscv/actions/workflows/do-nothing.yml/badge.svg)](https://github.com/Syakmoon/pintos-riscv/actions/workflows/do-nothing.yml)

This is a RISC-V port of the original x86 Pintos instructional operating
system. The version I used for implementation is Berkeley CS162 Fall 2021's
group project skeleton (commit `6c80e04`).

Prerequisites
=============

### Linux
- GNU make
- [RISC-V GNU Compiler Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain). 32-bit or multilib (recommended) version is required
- QEMU >= 7.1.0 (If not included in the toolchain)
- Perl >= 5.8.0
- Python >= 3.6.0, python-is-python3
- fzf
- cgdb >= 0.8.0

Usage
=====

1. Add path to `src/utils` to your PATH, or copy "backtrace", "pintos",
"pintos-gdb", "pintos-mkdisk", "pintos-set-cmdline", "pintos-test" and
"Pintos.pm" to one of your PATH directories.
2. Inside "pintos-gdb", change the definition of `GDBMACROS` to point
to `src/misc/gdb-macros`.

The RISC-V Pintos should now work the same way as x86 Pintos. Refer to
Berkeley CS 162's website or Stanford CS 140 (Virtual Memory part untested)'s
website for further instructions on how to run, debug, test Pintos.

But now, you should be able to run `make` in any project directory, such as
`src/userprog`, and run the `do-nothing` test by running
`pintos-test do-nothing`. The x86 Pintos will fail on this test because user
program arguments are not pushed onto the user stack yet, but the RISC-V ABIs
passes arguments in general-purpose registers, so it naturally passes the test.

TODO
====

- [x] Machine mode loader
- [x] Trap handler
- [x] Timer interrupt
- [x] Virtio block device driver
- [x] NS16550a UART driver
- [x] Paging
- [x] RTC driver
- [x] Thread, userprog assembly code
- [ ] Keyboard, speaker, and pintos-fun drivers
- [ ] Further modify pintos, gdb-macros
- [ ] Port to RISC-V 64-bit

Known issues
============
- `double` arguments passed to `printf` will be read in a wrong byte order.
- On some versions of QEMU, if `-m` is 4 MB (Pintos default), fdt will overlap.

Screenshots
===========

### Running `do-nothing`
<img width="1054" src="https://github.com/Syakmoon/pintos-riscv/assets/43796875/751bb695-0ab4-4bd8-9870-ac10370ebe90">

### With complete CS 162 code
| Userprog | Threads | Filesys |
| -------- | ------- | ------- |
| <img width="288" src="https://github.com/Syakmoon/pintos-riscv/assets/43796875/62b5b8e5-f82b-4da1-962f-c57db7e6314d"> | <img width="345" src="https://github.com/Syakmoon/pintos-riscv/assets/43796875/05d1a2b0-6342-456f-a8b1-04c0511138e6"> | <img width="395" src="https://github.com/Syakmoon/pintos-riscv/assets/43796875/5b5a3b9b-59b2-4a74-ba7b-9cd9673b3bc1"> |
