/* Deliberately faulting "victim" for windows_dwarf_unwind_safety_net_test
 * (see that file's own comment for the full story). Compiled with an
 * explicit -fdwarf-exceptions flag (see this file's own CMakeLists.txt
 * custom_command) so its non-leaf functions carry NO Windows-native
 * `.pdata`/`.xdata` unwind info at all -- exactly the real-world shape
 * TODO.md item 7 / docs/cxx_runtime.md's "Known cost" section describes
 * for this project's own DWARF-compiled C++ runtime, reproduced here in
 * plain C (no libc++/libcxx dependency needed at all: -fdwarf-exceptions
 * affects codegen/unwind-table emission the same way regardless of
 * language) so this stays in the default ctest suite rather than gated
 * behind the separate, opt-in CRT_USE_IMPORTED_LIBCXX pipeline.
 *
 * Deliberately never linked against this project's own libc: built via
 * tools/crt-cc with no CRT_SYSROOT libc dependency beyond what crt1.o
 * itself needs, matching pseudo_reloc.c's own "cannot assume libc state
 * is safe" discipline -- this file's whole point is to crash before
 * anything else can matter. mainCRTStartup()'s startup sequence (crt1.c)
 * installs the vectored-exception safety net (dwarf_unwind_safety_net.c)
 * before this program's own main() ever runs, so nothing in main() needs
 * to touch it directly -- the fault below is caught (or not) entirely by
 * that already-installed, process-wide handler.
 *
 * volatile local buffers + __attribute__((noinline)) force each function
 * to genuinely use stack space and remain non-leaf (not folded/inlined
 * away by the optimizer), matching the exact shape that was empirically
 * confirmed (2026-08-21, see HISTORY.md's dated entry) to reproduce the
 * "OS second-chance SEH search corrupts crossing an untabled DWARF
 * frame" gap in a standalone repro before this file existed. */

volatile int sink;

__attribute__((noinline)) static void dwarf_leaf_fault(int depth) {
  volatile char buf[64];
  int i;
  int* p;

  for (i = 0; i < 64; i++) {
    buf[i] = (char)(depth + i);
  }
  sink += buf[0];
  p = (int*)(void*)(long long)depth; /* depth == 0 -> NULL */
  *p = 42;                           /* the hardware fault (access violation) */
}

__attribute__((noinline)) static void dwarf_mid2(int depth) {
  volatile char buf[64];
  int i;

  for (i = 0; i < 64; i++) {
    buf[i] = (char)(depth + i);
  }
  sink += buf[1];
  dwarf_leaf_fault(depth - 1);
  sink += buf[2];
}

__attribute__((noinline)) static void dwarf_mid1(int depth) {
  volatile char buf[64];
  int i;

  for (i = 0; i < 64; i++) {
    buf[i] = (char)(depth + i);
  }
  sink += buf[1];
  dwarf_mid2(depth - 1);
  sink += buf[2];
}

__attribute__((noinline)) static void dwarf_entry(int depth) {
  volatile char buf[64];
  int i;

  for (i = 0; i < 64; i++) {
    buf[i] = (char)(depth + i);
  }
  sink += buf[1];
  dwarf_mid1(depth - 1);
  sink += buf[2];
}

int main(void) {
  dwarf_entry(4); /* never returns: faults 3 frames deeper */
  return 0;       /* unreachable -- if this line is ever hit, the test fails */
}
