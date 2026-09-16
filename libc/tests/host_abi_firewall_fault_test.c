/* host_abi_firewall_fault_test -- TODO.md's "Allocator baseline
 * validation before Upper Runtime" tranche 7, the negative half: proves
 * a real Host ABI firewall violation (CRT calling its own free() on a
 * host-owned object) gets caught mechanically, not just avoided by code
 * review. Spawns host_abi_firewall_fault_victim (a real, deliberately-
 * crashing executable) and checks CRT_DEBUG_MALLOC actually trapped it.
 *
 * Identical crash-detection shape to tranche 6's malloc_fault_test.c --
 * see that file's own top comment for why this checks WIFEXITED(status)
 * && WEXITSTATUS(status) == 128 + SIGILL on Windows (confirmed
 * empirically there: an unhandled __builtin_trap() in this DWARF-
 * compiled code lands in windows_dwarf_unwind_safety_net.c's own
 * existing vectored exception handler). POSIX hosts require real signal
 * termination, accepting SIGILL or SIGTRAP because Clang's lowering is
 * architecture-dependent (Linux/x86_64 illegal instruction versus AArch64
 * BRK on both Linux and Darwin). */

#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  pid_t pid;
  int status = 0;
  char* argv[2];

  argv[0] = (char*)CRT_HOST_ABI_FIREWALL_FAULT_VICTIM_EXE;
  argv[1] = 0;

  if (posix_spawn(&pid, CRT_HOST_ABI_FIREWALL_FAULT_VICTIM_EXE, 0, 0, argv, environ) != 0) {
    fprintf(stderr, "host_abi_firewall_fault_test: spawn failed\n");
    return 1;
  }
  if (waitpid(pid, &status, 0) != pid) {
    fprintf(stderr, "host_abi_firewall_fault_test: wait failed\n");
    return 1;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGILL) {
    printf("host_abi_firewall_fault_test: ok\n");
    return 0;
  }
#else
  if (WIFSIGNALED(status) &&
      (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP)) {
    printf("host_abi_firewall_fault_test: ok\n");
    return 0;
  }
#endif

  if (WIFEXITED(status) && WEXITSTATUS(status) == 66) {
    fprintf(stderr, "host_abi_firewall_fault_test: diagnostic did not trap\n");
  } else if (WIFEXITED(status) && WEXITSTATUS(status) == 65) {
    fprintf(stderr, "host_abi_firewall_fault_test: victim setup failed\n");
  } else {
    fprintf(stderr, "host_abi_firewall_fault_test: victim did not fail the expected way (status=0x%x)\n",
            (unsigned int)status);
  }
  return 1;
}
