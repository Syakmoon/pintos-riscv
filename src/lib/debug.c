#include <debug.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wframe-address"

/* Prints the call stack, that is, a list of addresses, one in
   each of the functions we are nested within.  gdb or addr2line
   may be applied to kernel.o to translate these into file names,
   line numbers, and function names.  */
void debug_backtrace(void) {
  static bool explained;
  void** frame;

  printf("Call stack: %p", __builtin_return_address(0));

  /* __builtin_frame_address(1) is unreliable on RISC-V. */
  for (frame = __builtin_frame_address(0), frame = frame[-2];
       (uintptr_t)frame >= 0x1000 && (uintptr_t)frame[-2] & 0xfff;
       frame = frame[-2])
    printf(" %p", frame[-1]);
  printf(" %p", frame[-1]);
  printf(".\n");

  if (!explained) {
    explained = true;
    printf("The `backtrace' program can make call stacks useful.\n"
           "Read \"Backtraces\" in the \"Debugging Tools\" chapter\n"
           "of the Pintos documentation for more information.\n");
  }
}

#ifdef MACHINE
void debug_panic(const char* file, int line, const char* function, const char* message, ...) {
  va_list args;

  printf("Kernel PANIC at %s:%d in %s(): ", file, line, function);

  va_start(args, message);
  vprintf(message, args);
  printf("\n");
  va_end(args);

  debug_backtrace();

  shutdown();
  for (;;)
    ;
}
#endif

#pragma GCC diagnostic pop
