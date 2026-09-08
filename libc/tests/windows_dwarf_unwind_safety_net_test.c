/* Permanent regression test for libc/src/arch/windows/common/
 * dwarf_unwind_safety_net.c -- see that file's own top comment for the
 * full empirical background (TODO.md item 7 / docs/cxx_runtime.md's
 * "Known cost: DWARF-compiled code has zero Windows-native unwind
 * info"). This exercises the REAL production toolchain end to end, not
 * a synthetic clang-direct repro: windows_dwarf_unwind_safety_net_
 * victim.c is compiled through tools/crt-cc with an explicit
 * -fdwarf-exceptions flag (see this test's own CMakeLists.txt
 * custom_command), reproducing exactly the shape this project's own
 * DWARF-compiled C++ runtime has -- a chain of real, non-leaf, unwind-
 * table-less functions ending in a genuine hardware fault -- then this
 * driver spawns it and asserts the fault was turned into a controlled,
 * deterministic exit (128 + SIGSEGV) instead of whatever uncontrolled
 * behavior the OS's own second-chance search would otherwise produce
 * trying to walk back out through those untabled frames.
 *
 * The victim's own path is baked in at compile time via the
 * CRT_DWARF_SAFETY_NET_VICTIM_EXE string define (tests/CMakeLists.txt's
 * own target_compile_definitions() call) rather than passed as an argv:
 * add_crt_test()'s own internal add_test() call takes no extra COMMAND
 * arguments, and this is a genuinely separate, deliberately-crashing
 * binary, not a re-launch of this same test executable the way several
 * other tests in this directory use /proc/self/exe for.
 *
 * Windows-only: the whole mechanism this exercises is Windows-specific
 * (see dwarf_unwind_safety_net.c's own top comment for why Linux/macOS
 * never need anything like it -- their own DWARF-based unwinder IS the
 * OS-native mechanism there, unlike Windows' separate table-based SEH
 * model). Gated in tests/CMakeLists.txt the same way windows_pseudo_
 * reloc_test/windows_dll_symbol_priority_test already are. */

#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "windows_dwarf_unwind_safety_net_test: %s\n", message);
  return 1;
}

int main(void) {
  pid_t pid;
  int status = 0;
  char* child_argv[2];

  child_argv[0] = (char*)CRT_DWARF_SAFETY_NET_VICTIM_EXE;
  child_argv[1] = 0;

  if (posix_spawn(&pid, CRT_DWARF_SAFETY_NET_VICTIM_EXE, 0, 0, child_argv, environ) != 0) {
    return fail("spawn victim");
  }
  if (waitpid(pid, &status, 0) != pid) {
    return fail("wait victim");
  }
  /* The real assertion: a controlled, deterministic exit matching this
   * project's own "128 + POSIX signal number" convention (libc/src/
   * signal.c's abort(), dwarf_unwind_safety_net.c's own ExitProcess()
   * call).
   *
   * WEXITSTATUS(), not WTERMSIG()/WIFSIGNALED(): confirmed for real
   * (2026-08-21) that this project's own Windows waitpid()
   * (libc/src/arch/windows/common/syscall.c) always encodes the raw
   * GetExitCodeProcess() value as a plain WIFEXITED status
   * (`(exit_code & 0xff) << 8`), regardless of how the child actually
   * terminated -- there is no STATUS_*-pattern detection or signal
   * synthesis in it at all today. Mapping a real hardware-fault exit
   * back into WIFSIGNALED()/WTERMSIG() the way a POSIX host's own
   * waitpid() would is exactly the separate, larger, deliberately-not-
   * yet-decided "bridge SIGSEGV/SIGFPE/SIGILL through signal()/raise()"
   * question docs/signal_delivery.md's own "Next Steps" already tracks
   * (distinct from this file's own narrower "did the process reach a
   * controlled, correct-value exit at all" concern) -- this test checks
   * what this project's PAL genuinely produces today, not what a future,
   * still-undecided signal-bridging feature might produce later. */
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 128 + SIGSEGV) {
    return fail("victim did not exit via the controlled 128+SIGSEGV path");
  }

  printf("windows_dwarf_unwind_safety_net_test: ok\n");
  return 0;
}
