/* malloc_fault_test -- TODO.md's "Allocator baseline validation before
 * Upper Runtime" tranche 6, "add expected-fault tests for double free,
 * cross-instance owner mismatch, and canary damage." Spawns malloc_
 * fault_victim (a real, separately-built, deliberately-crashing
 * executable, matching windows_dwarf_unwind_safety_net_test.c's own
 * "spawn a genuine victim binary" pattern -- its path is baked in at
 * compile time via CRT_MALLOC_FAULT_VICTIM_EXE, tests/CMakeLists.txt's
 * own target_compile_definitions() call) once per fault kind and checks
 * that CRT_DEBUG_MALLOC actually trapped it via __builtin_trap(), which
 * lowers to a native trap/illegal-instruction fault, depending on host
 * compiler/ABI.
 *
 * On Linux, an unhandled trap is real, native SIGILL termination. On
 * macOS, Apple/Darwin reports the same __builtin_trap() as SIGTRAP
 * instead; both are real host signal termination, not a clean exit.
 *
 * On Windows, confirmed empirically (not assumed) while building this
 * test: malloc_fault_victim.c is compiled the same DWARF-exceptions,
 * no-Windows-native-unwind-info way the rest of this project's GNU-ABI
 * code is, so the fault lands squarely in windows_dwarf_unwind_safety_
 * net.c's own vectored exception handler (the same mechanism windows_
 * dwarf_unwind_safety_net_test.c's own precedent already checks) --
 * confirmed by running the victim directly and observing its own
 * "crt: fatal exception 0xc000001d (SIGILL) ... controlled, deterministic
 * exit" message, then `128 + SIGILL` as its real exit code. So this
 * project's own general "no unwind info at the fault site -> controlled
 * exit" safety net turns out to already cover an ordinary __builtin_
 * trap() fault too, not just the stack-overflow-during-unwind case that
 * mechanism was originally built for -- this project's own Windows
 * waitpid() always reports such an exit as WIFEXITED() with the raw exit
 * code (see that precedent test's own comment for why), so the Windows
 * check below is WIFEXITED(status) && WEXITSTATUS(status) == 128 +
 * SIGILL, not WIFSIGNALED(). */

#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int expect_trap(const char* fault_kind) {
  pid_t pid;
  int status = 0;
  char* argv[3];

  argv[0] = (char*)CRT_MALLOC_FAULT_VICTIM_EXE;
  argv[1] = (char*)fault_kind;
  argv[2] = 0;

  if (posix_spawn(&pid, CRT_MALLOC_FAULT_VICTIM_EXE, 0, 0, argv, environ) != 0) {
    fprintf(stderr, "malloc_fault_test: spawn failed for %s\n", fault_kind);
    return 0;
  }
  if (waitpid(pid, &status, 0) != pid) {
    fprintf(stderr, "malloc_fault_test: wait failed for %s\n", fault_kind);
    return 0;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGILL) {
    return 1;
  }
#elif defined(CRT_TARGET_OS_MACOS)
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTRAP) {
    return 1;
  }
#else
  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGILL) {
    return 1;
  }
#endif

  if (WIFEXITED(status) && WEXITSTATUS(status) == 66) {
    fprintf(stderr, "malloc_fault_test: %s: diagnostic did not trap\n", fault_kind);
  } else if (WIFEXITED(status) && (WEXITSTATUS(status) == 65 || WEXITSTATUS(status) == 2)) {
    fprintf(stderr, "malloc_fault_test: %s: victim setup failed (exit %d)\n", fault_kind, WEXITSTATUS(status));
  } else {
    fprintf(stderr, "malloc_fault_test: %s: victim did not fail the expected way (status=0x%x)\n", fault_kind,
            (unsigned int)status);
  }
  return 0;
}

int main(void) {
  if (!expect_trap("double-free") || !expect_trap("canary-damage") || !expect_trap("owner-mismatch")) {
    return 1;
  }

  printf("malloc_fault_test: ok\n");
  return 0;
}
