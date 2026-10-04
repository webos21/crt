#ifndef CRT_PRIVATE_CRT_LINUX_UCONTEXT_AARCH64_H
#define CRT_PRIVATE_CRT_LINUX_UCONTEXT_AARCH64_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

/* The Bionic/Linux aarch64 ucontext_t (bionic/libc/include/sys/ucontext.h; uc_mcontext is the kernel's
 * `struct sigcontext`) as a standalone definition. <ucontext.h> selects this layout only for a
 * consumer that sees the target as Linux (-D__linux__); this libc itself is compiled as a macOS
 * target, where that header gives get/setcontext a private layout. The macOS signal backend builds
 * this layout for an SA_SIGINFO handler (libc/src/arch/macos/common/signal_backend.c), so that
 * Linux-shaped code such as WebKit reads and edits the interrupted registers the way it does on
 * Linux; tests read it through the same definition. Only meaningful on aarch64. */
struct crt_linux_aarch64_sigcontext {
  uint64_t fault_address;
  uint64_t regs[31];
  uint64_t sp;
  uint64_t pc;
  uint64_t pstate;
  unsigned char reserved[4096] __attribute__((aligned(16)));
};

struct crt_linux_aarch64_ucontext {
  unsigned long uc_flags;
  struct crt_linux_aarch64_ucontext* uc_link;
  struct {
    void* ss_sp;
    int ss_flags;
    size_t ss_size;
  } uc_stack;
  sigset_t uc_sigmask;
  char padding[128 - sizeof(sigset_t)];
  struct crt_linux_aarch64_sigcontext uc_mcontext __attribute__((aligned(16)));
};

_Static_assert(offsetof(struct crt_linux_aarch64_ucontext, uc_mcontext) == 176, "ucontext_t layout");
_Static_assert(offsetof(struct crt_linux_aarch64_sigcontext, pc) == 264, "sigcontext layout");

#endif
