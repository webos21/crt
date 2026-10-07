#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include <private/crt_signal_backend.h>
#include <private/crt_signal_wait.h>
#include <private/crt_tls.h>

/* Windows signal backend.
 *
 * Windows has no kernel mechanism that delivers a POSIX signal to a chosen thread, so the CRT builds
 * the subset a multi-threaded runtime needs (WTF/JavaScriptCore's thread suspension and VM traps),
 * in this process only:
 *
 *  - every thread has its own signal mask and pending set (crt_signal_state, in the thread's context);
 *  - pthread_kill() to another thread SUSPENDS it, captures its CONTEXT, builds a frame on ITS stack
 *    (the saved CONTEXT, a Linux-layout ucontext_t, a siginfo_t), points its RIP at an entry stub and
 *    resumes it: the handler then runs on the target thread, and when it returns the (possibly
 *    rewritten) context is restored with RtlRestoreContext. A handler may therefore read the
 *    interrupted instruction pointer and move it, which is what a JIT's VM trap does to a compiled
 *    loop that never calls anything;
 *  - a hardware exception that has a meaning as a signal (int3 -> SIGTRAP, an access violation ->
 *    SIGSEGV, an illegal instruction -> SIGILL, a division by zero -> SIGFPE) reaches a handler
 *    installed for that signal through a vectored exception handler, with the same ucontext_t;
 *  - sigsuspend() really blocks (on the thread's event) and returns EINTR after a handler ran.
 *
 * What is NOT covered, on purpose: a thread that is inside the operating system's own wait/system-call
 * code (its RIP lies in ntdll, kernelbase, kernel32 or win32u) is not interrupted, because the kernel
 * would resume it into the handler with a stale system-call result; the signal stays pending and is
 * delivered the next time that thread unblocks or suspends in the CRT. So a thread parked in a plain
 * Win32 wait does not take a signal until it wakes. Process-directed signals (kill(), SIGCHLD,
 * process groups, Ctrl-C) are separate work and not part of this file's thread machinery; SIGCHLD
 * keeps its polled implementation below. See docs/signal_delivery.md, "Windows".
 *
 * SIGCHLD specifically: a process handle that becomes signaled on exit, already tracked per child in
 * syscall.c's child registry for waitpid(). __crt_windows_check_sigchld_pending() scans it; unblocking
 * SIGCHLD synchronously delivers an already-pending instance, matching real delivery on the way back
 * from the unblocking sigprocmask() on Linux/macOS (see docs/signal_delivery.md's "pselect() Atomicity",
 * which libc/src/poll.c's pselect() depends on). __crt_sys_poll()'s blocking loop makes the same check
 * on every iteration. Console control events (Ctrl-C) remain unbridged. */

#define CRT_WINAPI
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;

#define SIGNAL_MAX 32
#define STATUS_BREAKPOINT_CODE 0x80000003UL
#define STATUS_ACCESS_VIOLATION_CODE 0xC0000005UL
#define STATUS_ILLEGAL_INSTRUCTION_CODE 0xC000001DUL
#define STATUS_PRIVILEGED_INSTRUCTION_CODE 0xC0000096UL
#define STATUS_INT_DIVIDE_BY_ZERO_CODE 0xC0000094UL
#define STATUS_FLOAT_DIVIDE_BY_ZERO_CODE 0xC000008EUL
#define EXCEPTION_CONTINUE_EXECUTION_VALUE (-1L)
#define EXCEPTION_CONTINUE_SEARCH_VALUE 0L

/* Thread injection and hardware-fault mapping work on the thread's CONTEXT (x64 or ARM64, declared
 * below) and hand a handler the Linux register layout of the same architecture: Linux x86_64's
 * ucontext_t on x64, Bionic's aarch64 ucontext (private/crt_linux_ucontext_aarch64.h, as the macOS
 * backend does) on arm64. Another architecture would keep the per-thread masks, pending sets,
 * interruptible waits and sigsuspend(), with a directed signal staying pending until the thread next
 * unblocks or waits in the CRT. */
#if defined(__x86_64__) || defined(__aarch64__)
#define CRT_SIGNAL_INJECT 1
#endif

#if defined(__aarch64__)
#include <private/crt_linux_ucontext_aarch64.h>
typedef struct crt_linux_aarch64_ucontext crt_signal_ucontext;
#else
typedef ucontext_t crt_signal_ucontext;
#endif

#if defined(CRT_SIGNAL_INJECT)
#if defined(__x86_64__)
/* The x64 CONTEXT (winnt.h), declared here because the CRT never includes the Windows headers. */
struct crt_m128a {
  unsigned long long low;
  long long high;
} __attribute__((aligned(16)));

struct crt_xsave_format {
  unsigned short control_word;
  unsigned short status_word;
  unsigned char tag_word;
  unsigned char reserved1;
  unsigned short error_opcode;
  unsigned long error_offset;
  unsigned short error_selector;
  unsigned short reserved2;
  unsigned long data_offset;
  unsigned short data_selector;
  unsigned short reserved3;
  unsigned long mx_csr;
  unsigned long mx_csr_mask;
  struct crt_m128a float_registers[8];
  struct crt_m128a xmm_registers[16];
  unsigned char reserved4[96];
};

struct crt_win_context {
  unsigned long long p1_home, p2_home, p3_home, p4_home, p5_home, p6_home;
  unsigned long context_flags;
  unsigned long mx_csr;
  unsigned short seg_cs, seg_ds, seg_es, seg_fs, seg_gs, seg_ss;
  unsigned long eflags;
  unsigned long long dr0, dr1, dr2, dr3, dr6, dr7;
  unsigned long long rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
  unsigned long long r8, r9, r10, r11, r12, r13, r14, r15;
  unsigned long long rip;
  struct crt_xsave_format flt_save;
  struct crt_m128a vector_register[26];
  unsigned long long vector_control;
  unsigned long long debug_control;
  unsigned long long last_branch_to_rip;
  unsigned long long last_branch_from_rip;
  unsigned long long last_exception_to_rip;
  unsigned long long last_exception_from_rip;
} __attribute__((aligned(16)));

typedef char crt_win_context_size_check[sizeof(struct crt_win_context) == 1232 ? 1 : -1];
typedef char crt_win_context_flt_check[offsetof(struct crt_win_context, flt_save) == 0x100 ? 1 : -1];

#define CONTEXT_AMD64 0x00100000UL
#define CONTEXT_CONTROL_FLAG (CONTEXT_AMD64 | 0x1UL)
#define CONTEXT_INTEGER_FLAG (CONTEXT_AMD64 | 0x2UL)
#define CONTEXT_SEGMENTS_FLAG (CONTEXT_AMD64 | 0x4UL)
#define CONTEXT_FLOATING_POINT_FLAG (CONTEXT_AMD64 | 0x8UL)
#define CONTEXT_CAPTURE (CONTEXT_CONTROL_FLAG | CONTEXT_INTEGER_FLAG | CONTEXT_SEGMENTS_FLAG | CONTEXT_FLOATING_POINT_FLAG)
#define CONTEXT_REDIRECT (CONTEXT_CONTROL_FLAG | CONTEXT_INTEGER_FLAG)
#define CTX_PC(c) ((c)->rip)
#define CTX_SP(c) ((c)->rsp)
#else
/* The ARM64 CONTEXT (winnt.h ARM64_NT_CONTEXT): x[0..28], fp (x29), lr (x30), then sp, pc, v0-v31. */
struct crt_m128a {
  unsigned long long low;
  long long high;
} __attribute__((aligned(16)));

struct crt_win_context {
  unsigned long context_flags;
  unsigned long cpsr;
  unsigned long long x[31];
  unsigned long long sp;
  unsigned long long pc;
  struct crt_m128a v[32];
  unsigned long fpcr;
  unsigned long fpsr;
  unsigned long bcr[8];
  unsigned long long bvr[8];
  unsigned long wcr[2];
  unsigned long long wvr[2];
} __attribute__((aligned(16)));

typedef char crt_win_context_size_check[sizeof(struct crt_win_context) == 912 ? 1 : -1];
typedef char crt_win_context_v_check[offsetof(struct crt_win_context, v) == 272 ? 1 : -1];

#define CONTEXT_ARM64 0x00400000UL
#define CONTEXT_CONTROL_FLAG (CONTEXT_ARM64 | 0x1UL)
#define CONTEXT_INTEGER_FLAG (CONTEXT_ARM64 | 0x2UL)
#define CONTEXT_FLOATING_POINT_FLAG (CONTEXT_ARM64 | 0x4UL)
#define CONTEXT_CAPTURE (CONTEXT_CONTROL_FLAG | CONTEXT_INTEGER_FLAG | CONTEXT_FLOATING_POINT_FLAG)
#define CONTEXT_REDIRECT (CONTEXT_CONTROL_FLAG | CONTEXT_INTEGER_FLAG)
#define CTX_PC(c) ((c)->pc)
#define CTX_SP(c) ((c)->sp)
#endif

struct crt_exception_record {
  DWORD exception_code;
  DWORD exception_flags;
  struct crt_exception_record* exception_record;
  void* exception_address;
  DWORD number_parameters;
  unsigned long long exception_information[15];
};

struct crt_exception_pointers {
  struct crt_exception_record* exception_record;
  struct crt_win_context* context_record;
};

struct crt_memory_info {
  void* base_address;
  void* allocation_base;
  DWORD allocation_protect;
  DWORD partition_id;
  size_t region_size;
  DWORD state;
  DWORD protect;
  DWORD type;
};

#endif

#if defined(CRT_SIGNAL_INJECT)
__declspec(dllimport) DWORD CRT_WINAPI SuspendThread(HANDLE thread);
__declspec(dllimport) DWORD CRT_WINAPI ResumeThread(HANDLE thread);
__declspec(dllimport) BOOL CRT_WINAPI GetThreadContext(HANDLE thread, struct crt_win_context* context);
__declspec(dllimport) BOOL CRT_WINAPI SetThreadContext(HANDLE thread, const struct crt_win_context* context);
#endif
__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentProcess(void);
__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentThread(void);
__declspec(dllimport) DWORD CRT_WINAPI GetCurrentThreadId(void);
__declspec(dllimport) BOOL CRT_WINAPI DuplicateHandle(HANDLE source_process, HANDLE source, HANDLE target_process,
                                                      HANDLE* target, DWORD access, BOOL inherit, DWORD options);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE handle);
__declspec(dllimport) HANDLE CRT_WINAPI CreateEventA(void* attributes, BOOL manual_reset, BOOL initial, const char* name);
__declspec(dllimport) BOOL CRT_WINAPI SetEvent(HANDLE event);
__declspec(dllimport) DWORD CRT_WINAPI WaitForSingleObject(HANDLE handle, DWORD milliseconds);
__declspec(dllimport) DWORD CRT_WINAPI WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all,
                                                              DWORD milliseconds);
__declspec(dllimport) int CRT_WINAPI QueryPerformanceCounter(long long* count);
__declspec(dllimport) int CRT_WINAPI QueryPerformanceFrequency(long long* frequency);
__declspec(dllimport) void CRT_WINAPI WakeByAddressAll(void* address);
__declspec(dllimport) BOOL CRT_WINAPI SwitchToThread(void);
__declspec(dllimport) void CRT_WINAPI Sleep(DWORD milliseconds);
__declspec(dllimport) HANDLE CRT_WINAPI CreateThread(void* attributes, size_t stack_size,
                                                     DWORD(CRT_WINAPI* start)(void*), void* parameter, DWORD flags,
                                                     DWORD* thread_id);
#if defined(CRT_SIGNAL_INJECT)
__declspec(dllimport) void* CRT_WINAPI AddVectoredExceptionHandler(unsigned long first,
                                                                   long(CRT_WINAPI* handler)(struct crt_exception_pointers*));
#endif
__declspec(dllimport) HANDLE CRT_WINAPI GetModuleHandleA(const char* name);
__declspec(dllimport) void* CRT_WINAPI GetProcAddress(HANDLE module, const char* name);
#if defined(CRT_SIGNAL_INJECT)
__declspec(dllimport) size_t CRT_WINAPI VirtualQuery(const void* address, struct crt_memory_info* buffer, size_t length);

#endif

#define THREAD_RIGHTS 0x001FFFFFUL /* THREAD_ALL_ACCESS */
#define DUPLICATE_SAME_ACCESS_FLAG 0x2UL
#define WAIT_TIMEOUT_CODE 0x102UL
#define MEM_COMMIT_STATE 0x1000UL
#define PAGE_GUARD_FLAG 0x100UL
#define PAGE_NOACCESS_FLAG 0x01UL

/* Implemented in libc/src/arch/windows/common/syscall.c: scans the child registry for an unblocked,
 * previously-unobserved exited child. Returns 1 (and marks it observed) if found, 0 otherwise. */
int __crt_windows_check_sigchld_pending(void);

/* ---- per-signal dispositions (the backend's copy: signal.c keeps the handler itself) ---- */

static volatile int action_installed[SIGNAL_MAX];
static volatile unsigned long action_mask[SIGNAL_MAX];
static volatile int action_flags[SIGNAL_MAX];
#if defined(CRT_SIGNAL_INJECT)
static volatile long exception_handler_added;
#endif

static int signal_in_range(int sig) {
  return sig > 0 && sig < SIGNAL_MAX;
}

static unsigned long bit_of(int sig) {
  return 1UL << (unsigned int)(sig - 1);
}

/* ---- the calling thread's state ---- */

static crt_signal_state* current_state(void) {
  return &__crt_thread_get_current()->signal;
}

/* The initial thread's own state, for pthread_kill(initial thread). */
static crt_signal_state* initial_state;
static unsigned long initial_thread_id;

#if defined(CRT_SIGNAL_INJECT)
typedef long(CRT_WINAPI* restore_context_fn)(struct crt_win_context*, void*);
static restore_context_fn restore_context_pointer;

static restore_context_fn restore_context_function(void) {
  if (restore_context_pointer == 0) {
    HANDLE ntdll = GetModuleHandleA("ntdll.dll");

    if (ntdll != 0) {
      restore_context_pointer = (restore_context_fn)GetProcAddress(ntdll, "RtlRestoreContext");
    }
  }
  return restore_context_pointer;
}

#endif

/* Makes the calling thread known to the signal machinery: a handle other threads can suspend it
 * through, and the event its sigsuspend() waits on. Idempotent. */
#if defined(CRT_SIGNAL_INJECT)
static int in_operating_system_code(unsigned long long address);
static void registry_add(crt_signal_state* state);
static void registry_remove(crt_signal_state* state);
static void registry_reset(void);
#endif

void __crt_windows_signal_attach_current(void) {
  crt_signal_state* state = current_state();

  if (state->alive != 0 && state->thread_id == GetCurrentThreadId()) {
    return;
  }
  state->thread_id = GetCurrentThreadId();
  state->thread_handle = 0;
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &state->thread_handle,
                       THREAD_RIGHTS, 0, DUPLICATE_SAME_ACCESS_FLAG)) {
    state->thread_handle = 0;
  }
  if (state->wake_event == 0) {
    state->wake_event = CreateEventA(0, 0, 0, 0);
  }
  state->pending = 0;
  __atomic_store_n(&state->alive, 1, __ATOMIC_RELEASE);
#if defined(CRT_SIGNAL_INJECT)
  registry_add(state);
  /* Everything the sender needs while a target is stopped is resolved now: GetModuleHandleA() and
   * GetProcAddress() take the loader lock, which the stopped thread may be holding. */
  restore_context_function();
  (void)in_operating_system_code(0);
#endif
}

unsigned long __crt_windows_signal_current_mask(void) {
  __crt_windows_signal_attach_current();
  return current_state()->mask;
}

/* The thread has ended (or is ending): nobody can signal it any more. */
void __crt_windows_signal_detach_current(void) {
  crt_signal_state* state = current_state();

#if defined(CRT_SIGNAL_INJECT)
  registry_remove(state);
#endif
  __atomic_store_n(&state->alive, 0, __ATOMIC_RELEASE);
}

void __crt_windows_signal_attach_initial(void) {
  __crt_windows_signal_attach_current();
  initial_state = current_state();
  initial_thread_id = GetCurrentThreadId();
}

/* In a fork child the only thread is the one that called fork(): the other threads' handles do not
 * exist there. */
void __crt_windows_signal_after_fork_child(void) {
  crt_signal_state* state = current_state();

#if defined(CRT_SIGNAL_INJECT)
  registry_reset();
#endif

  state->alive = 0;
  state->thread_handle = 0;
  state->wake_event = 0;
  initial_state = 0;
  __crt_windows_signal_attach_current();
  initial_state = state;
  initial_thread_id = GetCurrentThreadId();
}

/* ---- delivering a signal on the calling thread ---- */

static unsigned long mask_for_handler(int sig, unsigned long current) {
  unsigned long mask = current | action_mask[sig];

  if ((action_flags[sig] & SA_NODEFER) == 0) {
    mask |= bit_of(sig);
  }
  return mask;
}

static void fill_siginfo(siginfo_t* info, int sig, int code) {
  memset(info, 0, sizeof(*info));
  info->si_signo = sig;
  info->si_code = code;
  info->si_pid = getpid();
  info->si_uid = geteuid();
}

#define SI_TKILL_CODE (-6)

/* Runs the handler for `sig` right here, on this thread's stack: the mask while it runs is the old
 * mask plus the handler's sa_mask (and the signal itself unless SA_NODEFER), restored afterwards. */
static void run_handler_now(int sig, const siginfo_t* info, void* context) {
  crt_signal_state* state = current_state();
  unsigned long old_mask = state->mask;
  crt_signal_ucontext blank;

  if (context == 0) {
    /* No interrupted context to show (the signal is delivered from a call, not by interruption): an
     * SA_SIGINFO handler still gets a readable, empty ucontext_t rather than a null pointer. */
    memset(&blank, 0, sizeof(blank));
    context = &blank;
  }
  state->mask = mask_for_handler(sig, old_mask);
  __crt_signal_dispatch_info(sig, info, context);
  __atomic_fetch_add(&state->handler_runs, 1, __ATOMIC_SEQ_CST);
  state->mask = old_mask;
}

/* Delivers every pending, unblocked signal to the calling thread (POSIX: on the way out of a call that
 * unblocked it). */
static void deliver_pending_with(void* context) {
  crt_signal_state* state = current_state();

  /* While this runs the thread has made up its own mind about what to deliver: an interruption sent
   * now would deliver the same signal a second time when the thread resumes here (inject() refuses). */
  __atomic_fetch_add(&state->delivering, 1, __ATOMIC_SEQ_CST);
  for (;;) {
    unsigned long deliverable = state->pending & ~state->mask;
    int sig;
    siginfo_t info;

    if (deliverable == 0) {
      __atomic_fetch_sub(&state->delivering, 1, __ATOMIC_SEQ_CST);
      return;
    }
    for (sig = 1; sig < SIGNAL_MAX; ++sig) {
      if ((deliverable & bit_of(sig)) != 0) {
        break;
      }
    }
    __atomic_fetch_and(&state->pending, ~bit_of(sig), __ATOMIC_SEQ_CST);
    fill_siginfo(&info, sig, SI_TKILL_CODE);
    run_handler_now(sig, &info, context);
  }
}

static void deliver_pending_now(void) {
  deliver_pending_with(0);
}

#if defined(CRT_SIGNAL_INJECT)
/* ---- the interrupted thread's frame ---- */

struct inject_frame {
  struct crt_win_context saved;
  crt_signal_ucontext uc;
  siginfo_t info;
  int sig;
  unsigned long saved_mask;
  crt_signal_state* state;
  volatile long* acknowledged; /* stable on the sender's stack until inject_locked() returns */
  volatile long claim; /* 0 waiting, 1 the handler took it, 2 the sender gave it up (see inject()) */
};

#if defined(__x86_64__)
static void context_to_ucontext(const struct crt_win_context* from, crt_signal_ucontext* uc, unsigned long mask) {
  greg_t* g = uc->uc_mcontext.gregs;

  memset(uc, 0, sizeof(*uc));
  g[REG_R8] = (greg_t)from->r8;
  g[REG_R9] = (greg_t)from->r9;
  g[REG_R10] = (greg_t)from->r10;
  g[REG_R11] = (greg_t)from->r11;
  g[REG_R12] = (greg_t)from->r12;
  g[REG_R13] = (greg_t)from->r13;
  g[REG_R14] = (greg_t)from->r14;
  g[REG_R15] = (greg_t)from->r15;
  g[REG_RDI] = (greg_t)from->rdi;
  g[REG_RSI] = (greg_t)from->rsi;
  g[REG_RBP] = (greg_t)from->rbp;
  g[REG_RBX] = (greg_t)from->rbx;
  g[REG_RDX] = (greg_t)from->rdx;
  g[REG_RAX] = (greg_t)from->rax;
  g[REG_RCX] = (greg_t)from->rcx;
  g[REG_RSP] = (greg_t)from->rsp;
  g[REG_RIP] = (greg_t)from->rip;
  g[REG_EFL] = (greg_t)from->eflags;
  g[REG_CSGSFS] = (greg_t)((unsigned long long)from->seg_cs | ((unsigned long long)from->seg_gs << 16) |
                           ((unsigned long long)from->seg_fs << 32));
  g[REG_OLDMASK] = (greg_t)mask;
  /* The FXSAVE image the Linux ucontext_t carries is the CONTEXT's own FltSave. */
  memcpy(&uc->__fpregs_mem, &from->flt_save, sizeof(uc->__fpregs_mem) < sizeof(from->flt_save)
                                                  ? sizeof(uc->__fpregs_mem)
                                                  : sizeof(from->flt_save));
  uc->uc_mcontext.fpregs = &uc->__fpregs_mem;
  uc->uc_sigmask = (sigset_t)mask;
}

/* What a handler may have changed: the general registers, the stack pointer, the instruction
 * pointer and the flags. */
static void ucontext_to_context(const crt_signal_ucontext* uc, struct crt_win_context* to) {
  const greg_t* g = uc->uc_mcontext.gregs;

  to->r8 = (unsigned long long)g[REG_R8];
  to->r9 = (unsigned long long)g[REG_R9];
  to->r10 = (unsigned long long)g[REG_R10];
  to->r11 = (unsigned long long)g[REG_R11];
  to->r12 = (unsigned long long)g[REG_R12];
  to->r13 = (unsigned long long)g[REG_R13];
  to->r14 = (unsigned long long)g[REG_R14];
  to->r15 = (unsigned long long)g[REG_R15];
  to->rdi = (unsigned long long)g[REG_RDI];
  to->rsi = (unsigned long long)g[REG_RSI];
  to->rbp = (unsigned long long)g[REG_RBP];
  to->rbx = (unsigned long long)g[REG_RBX];
  to->rdx = (unsigned long long)g[REG_RDX];
  to->rax = (unsigned long long)g[REG_RAX];
  to->rcx = (unsigned long long)g[REG_RCX];
  to->rsp = (unsigned long long)g[REG_RSP];
  to->rip = (unsigned long long)g[REG_RIP];
  to->eflags = (unsigned long)g[REG_EFL];
}

#else
#define CTX_UC_FPSIMD_MAGIC 0x46508001U

static void context_to_ucontext(const struct crt_win_context* from, crt_signal_ucontext* uc, unsigned long mask) {
  struct crt_linux_aarch64_sigcontext* m = &uc->uc_mcontext;
  unsigned int* fpsimd = (unsigned int*)m->reserved;
  int i;

  memset(uc, 0, sizeof(*uc));
  for (i = 0; i < 31; ++i) {
    m->regs[i] = from->x[i];
  }
  m->sp = from->sp;
  m->pc = from->pc;
  m->pstate = from->cpsr;
  /* The FP/SIMD state, as the kernel's fpsimd_context record: head{magic,size}, fpsr, fpcr, v0-v31. */
  fpsimd[0] = CTX_UC_FPSIMD_MAGIC;
  fpsimd[1] = 8 + 8 + 32 * 16;
  fpsimd[2] = from->fpsr;
  fpsimd[3] = from->fpcr;
  memcpy(m->reserved + 16, from->v, 32 * 16);
  uc->uc_sigmask = (sigset_t)mask;
}

/* What a handler may have changed: the general registers, the stack pointer, the program counter
 * and the flags. */
static void ucontext_to_context(const crt_signal_ucontext* uc, struct crt_win_context* to) {
  const struct crt_linux_aarch64_sigcontext* m = &uc->uc_mcontext;
  int i;

  for (i = 0; i < 31; ++i) {
    to->x[i] = m->regs[i];
  }
  to->sp = m->sp;
  to->pc = m->pc;
  to->cpsr = (unsigned long)m->pstate;
}

#endif

/* Runs the handler of an injected signal and leaves the (possibly rewritten) interrupted context in
 * frame->saved. The sender already put the handler's mask in place (so no second signal could slip in
 * between the interruption and this point); the handler runs under it and the mask goes back afterwards. */
static void run_injected_handler(struct inject_frame* frame) {
  crt_signal_state* state = frame->state;
  long unclaimed = 0;

  /* A sender that waited too long for this to start took the interruption back: do nothing, the
   * interrupted context is simply resumed. */
  if (!__atomic_compare_exchange_n(&frame->claim, &unclaimed, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    return;
  }
  __atomic_store_n(frame->acknowledged, 1, __ATOMIC_SEQ_CST);

  /* Keep senders from nesting another injected handler after the mask is restored but before this
   * handler has finished converting and restoring the interrupted context. A signal arriving in that
   * interval stays pending and is consumed by deliver_pending_with() below. */
  __atomic_fetch_add(&state->delivering, 1, __ATOMIC_SEQ_CST);
  __crt_signal_dispatch_info(frame->sig, &frame->info, &frame->uc);
  __atomic_fetch_add(&state->handler_runs, 1, __ATOMIC_SEQ_CST);
  state->mask = frame->saved_mask;
  /* A signal that arrived (and was held back by the mask) while the handler ran sees the same
   * interrupted context, which the handler may rewrite too. */
  deliver_pending_with(&frame->uc);
  ucontext_to_context(&frame->uc, &frame->saved);
  __atomic_fetch_sub(&state->delivering, 1, __ATOMIC_SEQ_CST);
}

void __crt_windows_signal_run(struct inject_frame* frame) {
  restore_context_fn restore = restore_context_function();

  run_injected_handler(frame);
  if (restore != 0) {
    restore(&frame->saved, 0);
  }
  _exit(128 + SIGABRT); /* the context could not be restored: no way back */
}

#if defined(__x86_64__)
/* The stub a suspended thread is redirected to: %rcx is the frame, %rsp is 8 mod 16 as after a call. */
__asm__(".text\n"
        ".globl __crt_windows_signal_entry\n"
        ".def __crt_windows_signal_entry; .scl 2; .type 32; .endef\n"
        ".p2align 4, 0x90\n"
        "__crt_windows_signal_entry:\n"
        "  subq $40, %rsp\n"
        "  call __crt_windows_signal_run\n"
        "  ud2\n");
#else
/* The stub a suspended thread is redirected to: x0 is the frame, sp is 16-byte aligned. */
__asm__(".text\n"
        ".globl __crt_windows_signal_entry\n"
        ".def __crt_windows_signal_entry; .scl 2; .type 32; .endef\n"
        ".p2align 2\n"
        "__crt_windows_signal_entry:\n"
        "  bl __crt_windows_signal_run\n"
        "  brk #0\n");
#endif
extern char __crt_windows_signal_entry[];

/* Whether `address` lies inside one of the operating system's own wait / system-call modules. */
static int in_operating_system_code(unsigned long long address) {
  static const char* const modules[] = {"ntdll.dll", "kernelbase.dll", "kernel32.dll", "win32u.dll"};
  static unsigned long long base[4];
  static unsigned long long size[4];
  static int resolved;
  int i;

  if (!resolved) {
    for (i = 0; i < 4; ++i) {
      HANDLE module = GetModuleHandleA(modules[i]);

      if (module != 0) {
        const unsigned char* image = (const unsigned char*)module;
        unsigned int pe = *(const unsigned int*)(image + 0x3c);

        base[i] = (unsigned long long)(uintptr_t)image;
        size[i] = *(const unsigned int*)(image + pe + 24 + 56); /* OptionalHeader.SizeOfImage */
      }
    }
    __atomic_store_n(&resolved, 1, __ATOMIC_RELEASE);
  }
  for (i = 0; i < 4; ++i) {
    if (base[i] != 0 && address >= base[i] && address < base[i] + size[i]) {
      return 1;
    }
  }
  return 0;
}

/* The address range [low, high) can be written by the sender: committed, writable, not a guard page. */
static int writable_range(const void* low, size_t length) {
  const unsigned char* cursor = (const unsigned char*)low;
  const unsigned char* end = cursor + length;

  while (cursor < end) {
    struct crt_memory_info info;
    const unsigned char* region_end;

    if (VirtualQuery(cursor, &info, sizeof(info)) == 0 || info.state != MEM_COMMIT_STATE ||
        (info.protect & (PAGE_GUARD_FLAG | PAGE_NOACCESS_FLAG)) != 0 || (info.protect & 0xCCUL) == 0) {
      return 0;
    }
    region_end = (const unsigned char*)info.base_address + info.region_size;
    cursor = region_end;
  }
  return 1;
}

static unsigned long long clock_microseconds(void);

/* A thread that was inside a hardware exception when it was redirected can carry on with the context it
 * had already been given and never run the stub: the signal would be lost, with the handler's mask left
 * in place (and a thread-suspend request waiting for ever for its answer). The context the system reports
 * for such a thread already shows the redirection, so the only evidence is that the handler does not
 * start. After the thread is let go, wait for an acknowledgement stored on the sender's stack (a thread
 * that was redirected but must wait for a CPU takes up to a few scheduler quanta). The claim inside the
 * target-stack frame cannot be the acknowledgement: once the handler restores the original stack pointer,
 * the target may reuse and overwrite that memory before the sender observes it. If no acknowledgement
 * arrives within a quarter of a second, cancel the frame (the handler, should it run after all, sees that and
 * only resumes the interrupted context) and take the interruption back: the mask and pending bit return to
 * what they were. Returns 1 when the handler has started, 0 when the interruption was taken back. */
static int interruption_took_effect(crt_signal_state* target, struct inject_frame* frame,
                                    volatile long* acknowledged, int sig, unsigned long old_mask) {
  unsigned long long spin_until = clock_microseconds() + 2000;
  unsigned long long deadline = clock_microseconds() + 250000; /* a redirected thread may wait for a CPU */
  long unclaimed = 0;
  unsigned long handler_mask = mask_for_handler(sig, old_mask);

  while (__atomic_load_n(acknowledged, __ATOMIC_SEQ_CST) == 0) {
    if (clock_microseconds() >= deadline) {
      if (__atomic_compare_exchange_n(&frame->claim, &unclaimed, 2, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        unsigned long expected = handler_mask;

        __atomic_compare_exchange_n(&target->mask, &expected, old_mask, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        __atomic_fetch_or(&target->pending, bit_of(sig), __ATOMIC_SEQ_CST);
        return 0;
      }
      /* The handler claimed it but may have been preempted before its next instruction stores the
       * acknowledgement. This stack slot must remain alive until that store really happened. */
      while (__atomic_load_n(acknowledged, __ATOMIC_SEQ_CST) == 0) {
        SwitchToThread();
      }
      break;
    }
    if (clock_microseconds() < spin_until) {
      SwitchToThread();
    } else {
      Sleep(1);
    }
  }
  return 1;
}

/* Interrupts `target` (not the caller) with `sig`. Returns 1 when the handler was set up on the thread,
 * 0 when it could not be done safely now (the signal is left pending), -1 when the thread is gone. */
static int inject_locked(crt_signal_state* target, int sig, unsigned long long* where) {
  struct crt_win_context context;
  struct inject_frame* frame;
  unsigned long long frame_address;
  unsigned long old_mask;
  HANDLE handle = target->thread_handle;
  volatile long acknowledged = 0;
  int result = 0;

  if (handle == 0 || restore_context_function() == 0) {
    return 0;
  }
  if (SuspendThread(handle) == (DWORD)-1) {
    return -1;
  }
  memset(&context, 0, sizeof(context));
  context.context_flags = CONTEXT_CAPTURE;
  if (!GetThreadContext(handle, &context)) {
    ResumeThread(handle);
    return -1;
  }
  if (in_operating_system_code(CTX_PC(&context))) {
    *where = CTX_PC(&context) ^ (CTX_SP(&context) * 31ULL);
    ResumeThread(handle);
    return 2; /* not now: the caller may try again a moment later */
  }
  if (__atomic_load_n(&target->delivering, __ATOMIC_SEQ_CST) != 0) {
    *where = 0;
    ResumeThread(handle);
    return 2; /* it is delivering for itself: look again in a moment (and the pending bit may be gone) */
  }
  if ((target->mask & bit_of(sig)) != 0 || (__atomic_load_n(&target->pending, __ATOMIC_SEQ_CST) & bit_of(sig)) == 0) {
    /* Blocked, or already taken by the thread itself (sigsuspend() or a wait consumed it between the
     * sender's mark and now): the target is stopped, so this is final, and a second delivery would run
     * the handler twice for one signal. */
    ResumeThread(handle);
    return 0;
  }
  /* No red zone on Windows, but a margin below the stack pointer costs nothing. */
  frame_address = (CTX_SP(&context) - 128 - sizeof(struct inject_frame)) & ~15ULL;
  if (!writable_range((const void*)(uintptr_t)frame_address, sizeof(struct inject_frame) + 128)) {
    ResumeThread(handle);
    return 0;
  }
  frame = (struct inject_frame*)(uintptr_t)frame_address;
  old_mask = target->mask;
  frame->saved = context;
  fill_siginfo(&frame->info, sig, SI_TKILL_CODE);
  frame->sig = sig;
  frame->saved_mask = old_mask;
  frame->state = target;
  frame->acknowledged = &acknowledged;
  frame->claim = 0;
  context_to_ucontext(&context, &frame->uc, old_mask);
  /* The handler's mask goes in now, while the thread cannot run: nothing can be delivered to it
   * between the interruption and the handler's first instruction. */
  target->mask = mask_for_handler(sig, old_mask);
  __atomic_fetch_and(&target->pending, ~bit_of(sig), __ATOMIC_SEQ_CST);

  context.context_flags = CONTEXT_REDIRECT;
#if defined(__x86_64__)
  context.rsp = frame_address - 8;
  context.rip = (unsigned long long)(uintptr_t)__crt_windows_signal_entry;
  context.rcx = (unsigned long long)(uintptr_t)frame;
#else
  context.sp = frame_address;
  context.pc = (unsigned long long)(uintptr_t)__crt_windows_signal_entry;
  context.x[0] = (unsigned long long)(uintptr_t)frame;
  context.x[30] = 0; /* nothing returns into the interrupted code; the stub never falls out */
#endif
  if (SetThreadContext(handle, &context)) {
    result = 1;
  } else {
    target->mask = old_mask;
    __atomic_fetch_or(&target->pending, bit_of(sig), __ATOMIC_SEQ_CST);
  }
  ResumeThread(handle);
  if (result == 1 && !interruption_took_effect(target, frame, &acknowledged, sig, old_mask)) {
    return 2; /* the thread went on in its own code: nothing was delivered, the caller may try again */
  }
  return result;
}

/* One interruption of a thread at a time, from the first stop to the handler having started: a second
 * sender (the pump and the caller of pthread_kill race each other) would capture the mask the first one had
 * just put in place and the handler would give the thread back that mask for good. */
static int inject(crt_signal_state* target, int sig, unsigned long long* where) {
  int result;

  if (__atomic_exchange_n(&target->injecting, 1, __ATOMIC_SEQ_CST) != 0) {
    *where = 0;
    return 2; /* someone else is at it: look again in a moment */
  }
  result = inject_locked(target, sig, where);
  __atomic_store_n(&target->injecting, 0, __ATOMIC_SEQ_CST);
  return result;
}

#else
/* No thread injection on this architecture: the signal stays pending (see the note at the top). */
static int inject(crt_signal_state* target, int sig, unsigned long long* where) {
  (void)target;
  (void)sig;
  *where = 0;
  return 0;
}
#endif

#if defined(CRT_SIGNAL_INJECT)
/* ---- the signal pump ----
 *
 * The kernel delivers a signal to a thread the moment it is back in user mode, however long that takes.
 * Here delivery is an interruption done by another thread, and a thread that is almost always inside an
 * operating-system leaf (a spin loop through TlsGetValue behind errno, say) can be missed by a bounded
 * number of tries; the signal would then stay pending until the thread happens to wait, which a
 * thread-suspend request (JavaScriptCore's) never lets it do. So a sender that gives up hands the signal
 * to this thread, which keeps trying every millisecond for as long as a deliverable signal is pending. */
#define REGISTRY_SLOTS 4096

static crt_signal_state* registry_slots[REGISTRY_SLOTS];
static volatile long registry_lock;
static HANDLE pump_event;
static volatile long pump_started;

static void registry_acquire(void) {
  while (__atomic_exchange_n(&registry_lock, 1, __ATOMIC_ACQUIRE) != 0) {
    SwitchToThread();
  }
}

static void registry_release(void) {
  __atomic_store_n(&registry_lock, 0, __ATOMIC_RELEASE);
}

static void registry_add(crt_signal_state* state) {
  int i;

  registry_acquire();
  for (i = 0; i < REGISTRY_SLOTS; ++i) {
    if (registry_slots[i] == state) {
      break;
    }
    if (registry_slots[i] == 0) {
      registry_slots[i] = state;
      break;
    }
  }
  registry_release();
}

/* Waits for a pump pass that is looking at the state to finish: the caller frees the state next. */
static void registry_remove(crt_signal_state* state) {
  int i;

  registry_acquire();
  for (i = 0; i < REGISTRY_SLOTS; ++i) {
    if (registry_slots[i] == state) {
      registry_slots[i] = 0;
      break;
    }
  }
  registry_release();
}

static void registry_reset(void) {
  memset(registry_slots, 0, sizeof(registry_slots));
  registry_lock = 0;
  pump_event = 0;
  pump_started = 0;
}

typedef long(CRT_WINAPI* set_timer_resolution_fn)(unsigned long, unsigned char, unsigned long*);

static DWORD CRT_WINAPI pump_main(void* argument) {
  HANDLE ntdll = GetModuleHandleA("ntdll.dll");
  set_timer_resolution_fn set_resolution =
      ntdll != 0 ? (set_timer_resolution_fn)GetProcAddress(ntdll, "NtSetTimerResolution") : 0;

  (void)argument;
  if (set_resolution != 0) {
    unsigned long actual = 0;

    set_resolution(10000, 1, &actual); /* 1 ms: Sleep(1) below should not take 15.6 ms */
  }
  for (;;) {
    int outstanding;
    unsigned long passes = 0;

    WaitForSingleObject(pump_event, 0xffffffffUL);
    do {
      int i;

      outstanding = 0;
      registry_acquire();
      for (i = 0; i < REGISTRY_SLOTS; ++i) {
        crt_signal_state* target = registry_slots[i];
        unsigned long deliverable;
        int sig;
        unsigned long long where = 0;

        if (target == 0 || __atomic_load_n(&target->alive, __ATOMIC_ACQUIRE) == 0) {
          continue;
        }
        deliverable = __atomic_load_n(&target->pending, __ATOMIC_SEQ_CST) & ~target->mask;
        if (deliverable == 0) {
          continue;
        }
        if (__atomic_load_n(&target->waiting, __ATOMIC_SEQ_CST) != 0) {
          if (target->wake_event != 0) {
            SetEvent(target->wake_event); /* sigsuspend() delivers it itself */
          }
          continue;
        }
        if (__atomic_load_n(&target->in_wait, __ATOMIC_SEQ_CST) != 0) {
          void* address = target->wait_address;

          if (address != 0) {
            WakeByAddressAll(address); /* the wait delivers it itself */
          }
          if (target->wake_event != 0) {
            SetEvent(target->wake_event);
          }
          outstanding = 1;
          continue;
        }
        for (sig = 1; sig < SIGNAL_MAX; ++sig) {
          if ((deliverable & bit_of(sig)) != 0) {
            break;
          }
        }
        if (inject(target, sig, &where) == 2) {
          outstanding = 1;
        } else if (((__atomic_load_n(&target->pending, __ATOMIC_SEQ_CST) & ~target->mask) != 0)) {
          outstanding = 1; /* more of them, or one that could not be set up just now */
        }
      }
      registry_release();
      if (outstanding) {
        /* A thread inside a short system call is back in user code within microseconds: look again at
         * once for a while, then settle to a millisecond. */
        if (++passes < 5000) {
          SwitchToThread();
        } else {
          Sleep(1);
        }
      }
    } while (outstanding);
  }
  return 0;
}

/* Hands the signals nobody could deliver yet to the pump (started on first use). */
static void wake_pump(void) {
  if (__atomic_exchange_n(&pump_started, 1, __ATOMIC_SEQ_CST) == 0) {
    pump_event = CreateEventA(0, 0, 0, 0);
    if (pump_event == 0 || CreateThread(0, 0x20000, pump_main, 0, 0, 0) == 0) {
      __atomic_store_n(&pump_started, 0, __ATOMIC_SEQ_CST);
      return;
    }
  }
  while (__atomic_load_n(&pump_event, __ATOMIC_ACQUIRE) == 0) {
    SwitchToThread(); /* another thread is still creating it */
  }
  SetEvent(pump_event);
}
#endif

/* ---- interruptible waits (see private/crt_signal_wait.h) ---- */

/* A monotonic clock in microseconds: the tick counter's 15.6 ms steps would end a short sleep early. */
static unsigned long long clock_microseconds(void) {
  static long long frequency;
  long long count = 0;

  if (frequency == 0) {
    long long value = 0;

    QueryPerformanceFrequency(&value);
    frequency = value > 0 ? value : 1;
  }
  QueryPerformanceCounter(&count);
  return (unsigned long long)(count / frequency) * 1000000ULL +
         (unsigned long long)(count % frequency) * 1000000ULL / (unsigned long long)frequency;
}

/* Whole milliseconds still to wait until `deadline` (microseconds), rounded up; 0 when it has passed. */
static DWORD milliseconds_until(unsigned long long deadline) {
  unsigned long long now = clock_microseconds();

  return now >= deadline ? 0 : (DWORD)((deadline - now + 999ULL) / 1000ULL);
}

int __crt_windows_signal_wait_begin(volatile void* address) {
  crt_signal_state* state = current_state();

  __crt_windows_signal_attach_current();
  state->wait_address = (void*)address;
  __atomic_store_n(&state->in_wait, 1, __ATOMIC_SEQ_CST);
  /* A sender sets `pending` and then looks at in_wait; this side sets in_wait and then looks at
   * `pending`: one of the two always sees the other, so a wake-up cannot be lost. */
  return (__atomic_load_n(&state->pending, __ATOMIC_SEQ_CST) & ~state->mask) != 0;
}

/* A ucontext_t describing the waiting call: its stack pointer and callee-saved registers. */
#if defined(__aarch64__)
static __attribute__((noinline)) void describe_waiting_call(crt_signal_ucontext* context) {
  unsigned long long saved[12];

  __asm__ volatile(
      "stp x19, x20, [%0, #0]\n\t"
      "stp x21, x22, [%0, #16]\n\t"
      "stp x23, x24, [%0, #32]\n\t"
      "stp x25, x26, [%0, #48]\n\t"
      "stp x27, x28, [%0, #64]\n\t"
      "stp x29, x30, [%0, #80]\n\t"
      :
      : "r"(saved)
      : "memory");
  memset(context, 0, sizeof(*context));
  memcpy(&context->uc_mcontext.regs[19], saved, sizeof(saved));
  context->uc_mcontext.sp = (unsigned long long)(uintptr_t)__builtin_frame_address(0);
  context->uc_mcontext.pc = saved[11];
}
#else
static void describe_waiting_call(crt_signal_ucontext* context) {
  getcontext(context);
}
#endif

int __crt_windows_signal_wait_end(void) {
  crt_signal_state* state = current_state();
  int ran = 0;

  __atomic_store_n(&state->in_wait, 0, __ATOMIC_SEQ_CST);
  if ((state->pending & ~state->mask) != 0) {
    crt_signal_ucontext context;

    describe_waiting_call(&context);
    context.uc_sigmask = (sigset_t)state->mask;
    deliver_pending_with(&context);
    ran = 1;
  }
  return ran;
}

void* __crt_windows_signal_wake_event(void) {
  return current_state()->wake_event;
}

unsigned long __crt_windows_signal_wait_handle(void* handle, unsigned long milliseconds) {
  unsigned long long deadline = milliseconds == 0xffffffffUL ? 0 : clock_microseconds() + milliseconds * 1000ULL + 1;

  for (;;) {
    HANDLE handles[2];
    DWORD count = 1;
    DWORD remaining = 0xffffffffUL;
    DWORD result;
    int blocked;

    if (deadline != 0) {
      remaining = milliseconds_until(deadline);
    }
    blocked = __crt_windows_signal_wait_begin(0);
    handles[0] = handle;
    handles[1] = __crt_windows_signal_wake_event();
    if (handles[1] != 0) {
      count = 2;
    }
    result = blocked ? 0x102UL : WaitForMultipleObjects(count, handles, 0, remaining);
    __crt_windows_signal_wait_end();
    if (result == 0) {
      return 0; /* the handle */
    }
    if (result == WAIT_TIMEOUT_CODE && !blocked) {
      return WAIT_TIMEOUT_CODE;
    }
    if (result != WAIT_TIMEOUT_CODE && result != 1) {
      return result; /* a failure */
    }
    /* Woken for a signal (its handler has run): wait again for what is left of the time. */
    if (deadline != 0 && milliseconds_until(deadline) == 0) {
      return WAIT_TIMEOUT_CODE;
    }
  }
}

int __crt_windows_signal_sleep(unsigned long milliseconds) {
  unsigned long long deadline = clock_microseconds() + milliseconds * 1000ULL;

  for (;;) {
    DWORD remaining = milliseconds_until(deadline);
    int blocked;
    int ran;

    if (remaining == 0) {
      return 0;
    }
    blocked = __crt_windows_signal_wait_begin(0);
    if (!blocked) {
      WaitForSingleObject(__crt_windows_signal_wake_event(), remaining);
    }
    ran = __crt_windows_signal_wait_end();
    if (ran) {
      return 1;
    }
  }
}

/* ---- the operations signal.c and pthread.c call ---- */

int __crt_signal_backend_set_action(int bionic_sig, enum crt_signal_backend_action action) {
  if (!signal_in_range(bionic_sig)) {
    return 0;
  }
  action_installed[bionic_sig] = action == CRT_SIGNAL_BACKEND_DISPATCH;
  return 0;
}

#if defined(CRT_SIGNAL_INJECT)
static long CRT_WINAPI exception_handler(struct crt_exception_pointers* pointers);
#endif

int __crt_signal_backend_set_action_ex(int bionic_sig, enum crt_signal_backend_action action,
                                       int flags, const sigset_t* mask) {
  if (signal_in_range(bionic_sig)) {
    action_flags[bionic_sig] = flags;
    action_mask[bionic_sig] = mask != 0 ? (unsigned long)*mask : 0UL;
    action_installed[bionic_sig] = action == CRT_SIGNAL_BACKEND_DISPATCH;
#if defined(CRT_SIGNAL_INJECT)
    if (action == CRT_SIGNAL_BACKEND_DISPATCH &&
        (bionic_sig == SIGTRAP || bionic_sig == SIGSEGV || bionic_sig == SIGILL || bionic_sig == SIGFPE) &&
        __atomic_exchange_n(&exception_handler_added, 1, __ATOMIC_SEQ_CST) == 0) {
      AddVectoredExceptionHandler(1, exception_handler);
    }
#endif
  }
  __crt_windows_signal_attach_current();
  return 0;
}

int __crt_signal_backend_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

int __crt_signal_backend_set_mask(int how, const sigset_t* set) {
  /* The per-thread mask is the real one here (a spawned child's POSIX_SPAWN_SETSIGMASK arrives through
   * this call, as does the bookkeeping copy's update): apply it to the calling thread. */
  return __crt_signal_backend_sigprocmask(how, set, 0);
}

int __crt_signal_backend_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  crt_signal_state* state = current_state();
  unsigned long old = state->mask;
  unsigned long requested;

  if (oldset != 0) {
    *oldset = (sigset_t)old;
  }
  if (set == 0) {
    return 0;
  }
  requested = (unsigned long)*set;
  if (how == SIG_BLOCK) {
    state->mask = old | requested;
  } else if (how == SIG_UNBLOCK) {
    state->mask = old & ~requested;
  } else if (how == SIG_SETMASK) {
    state->mask = requested;
  } else {
    errno = EINVAL;
    return -1;
  }
  /* SIGKILL/SIGSTOP cannot be blocked. */
  state->mask &= ~(bit_of(SIGKILL) | bit_of(SIGSTOP));
  if ((old & ~state->mask) != 0) {
    /* Something was unblocked: a pending SIGCHLD from a child that already exited, and any
     * signal another thread sent while it was blocked, is delivered before this call returns. */
    if (__crt_windows_check_sigchld_pending()) {
      __crt_signal_dispatch(SIGCHLD);
    }
    deliver_pending_now();
  }
  return 0;
}

int __crt_signal_backend_sigsuspend(const sigset_t* mask) {
  crt_signal_state* state = current_state();
  unsigned long old = state->mask;
  unsigned long runs = state->handler_runs;

  __crt_windows_signal_attach_current();
  state->mask = (unsigned long)*mask & ~(bit_of(SIGKILL) | bit_of(SIGSTOP));
  for (;;) {
    /* A handler that an interruption ran on this thread while it waited ends the wait too. */
    if (state->handler_runs != runs) {
      break;
    }
    if ((state->pending & ~state->mask) != 0) {
      deliver_pending_now();
      break;
    }
    if (__crt_windows_check_sigchld_pending() && (state->mask & bit_of(SIGCHLD)) == 0) {
      __crt_signal_dispatch(SIGCHLD);
      break;
    }
    __atomic_store_n(&state->waiting, 1, __ATOMIC_SEQ_CST);
    /* The signal may have become pending between the check and the flag: look once more. */
    if ((state->pending & ~state->mask) == 0 && state->wake_event != 0) {
      WaitForSingleObject(state->wake_event, 10);
    }
    __atomic_store_n(&state->waiting, 0, __ATOMIC_SEQ_CST);
  }
  state->mask = old;
  /* Held back by the temporary mask, deliverable under the restored one: runs before this returns. */
  deliver_pending_now();
  errno = EINTR;
  return -1;
}

/* pthread_kill(): send `sig` to the thread whose state is `target`. Returns 0 or an errno value. */
int __crt_windows_signal_send(crt_signal_state* target, int sig) {
  int result;
  int attempt;
  int stuck = 0;
  unsigned long long where = 0;

  if (target == 0 || __atomic_load_n(&target->alive, __ATOMIC_ACQUIRE) == 0) {
    return ESRCH;
  }
  if (sig == 0) {
    return 0;
  }
  if (!signal_in_range(sig)) {
    return EINVAL;
  }
  __atomic_fetch_or(&target->pending, bit_of(sig), __ATOMIC_SEQ_CST);
  if (target->wake_event != 0) {
    SetEvent(target->wake_event);
  }
  if ((target->mask & bit_of(sig)) != 0) {
    return 0;
  }
  if (__atomic_load_n(&target->waiting, __ATOMIC_SEQ_CST) != 0) {
    return 0; /* sigsuspend() delivers it itself */
  }
  if (__atomic_load_n(&target->in_wait, __ATOMIC_SEQ_CST) != 0) {
    void* address = target->wait_address;

    /* Parked in a CRT wait: the thread cannot be interrupted there, but it can be woken, and it
     * runs the handler itself when the wait returns (wait_end). */
    if (address != 0) {
      WakeByAddressAll(address);
    }
    return 0;
  }
  if (target == current_state()) {
    deliver_pending_now();
    return 0;
  }
  result = inject(target, sig, &where);
  /* A thread that spends its time in a leaf of the operating system (TlsGetValue behind errno and
   * pthread_self, say) is caught there most of the time: try again, a few hundred times at most, the
   * way a kernel retries until the thread is back in user code. Past that the signal stays pending. */
  for (attempt = 0; result == 2 && attempt < 4000; ++attempt) {
    unsigned long long previous = where;

    SwitchToThread();
    if ((__atomic_load_n(&target->pending, __ATOMIC_SEQ_CST) & bit_of(sig)) == 0 ||
        (target->mask & bit_of(sig)) != 0 || __atomic_load_n(&target->alive, __ATOMIC_ACQUIRE) == 0) {
      return 0; /* delivered meanwhile, blocked, or the thread ended */
    }
    if (__atomic_load_n(&target->in_wait, __ATOMIC_SEQ_CST) != 0) {
      void* address = target->wait_address;

      if (address != 0) {
        WakeByAddressAll(address);
      }
      return 0;
    }
    if (__atomic_load_n(&target->waiting, __ATOMIC_SEQ_CST) != 0) {
      return 0;
    }
    result = inject(target, sig, &where);
    /* The very same pc and sp again and again is a thread inside a system call (a write, a wait), not one
     * passing through a leaf: stop holding the caller up, the pump delivers the signal when the thread
     * is back in user code. */
    if (result == 2 && where == previous && ++stuck >= 24) {
      break;
    }
    if (where != previous) {
      stuck = 0;
    }
  }
#if defined(CRT_SIGNAL_INJECT)
  if (result == 2) {
    wake_pump(); /* still not deliverable: the pump keeps trying */
  }
#endif
  return result < 0 ? ESRCH : 0;
}

crt_signal_state* __crt_windows_signal_state_of_initial_thread(void) {
  return initial_state;
}

#if defined(CRT_SIGNAL_INJECT)
/* ---- hardware exceptions as signals ---- */

static int signal_for_exception(const struct crt_exception_record* record, int* code) {
  switch (record->exception_code) {
    case STATUS_BREAKPOINT_CODE:
      *code = 128; /* SI_KERNEL; TRAP_BRKPT is 1 but the kernel reports SI_KERNEL for int3 on x86 */
      return SIGTRAP;
    case STATUS_ACCESS_VIOLATION_CODE:
      *code = 1; /* SEGV_MAPERR */
      return SIGSEGV;
    case STATUS_ILLEGAL_INSTRUCTION_CODE:
      *code = 1; /* ILL_ILLOPC */
      return SIGILL;
    case STATUS_PRIVILEGED_INSTRUCTION_CODE:
      /* `hlt` and the other privileged instructions: a general-protection fault, which Linux delivers as
       * SIGSEGV with si_code SI_KERNEL and no address. JavaScriptCore patches `hlt` into compiled code
       * as its VM-trap instruction and expects exactly that (Signal::AccessFault). */
      *code = 128;
      return SIGSEGV;
    case STATUS_INT_DIVIDE_BY_ZERO_CODE:
      *code = 1; /* FPE_INTDIV */
      return SIGFPE;
    case STATUS_FLOAT_DIVIDE_BY_ZERO_CODE:
      *code = 3; /* FPE_FLTDIV */
      return SIGFPE;
    default:
      return 0;
  }
}

#if defined(CRT_SIGNAL_INJECT)
#if defined(__x86_64__)
#define CTX_FIRST_ARGUMENT(c) ((c)->rcx)
#else
#define CTX_FIRST_ARGUMENT(c) ((c)->x[0])
#endif
#endif

static long CRT_WINAPI exception_handler(struct crt_exception_pointers* pointers) {
  int code = 0;
  int sig;

#if defined(CRT_SIGNAL_INJECT)
  if (pointers->exception_record->exception_address == (void*)__crt_windows_signal_entry) {
    /* A hardware fault the target had already taken when the sender redirected it: the kernel was still
     * building the exception, so it was dispatched with the stub's address as the fault site (and the
     * original fault's record), which no handler could recognise. Nothing was executed at the stub. Run
     * the injected handler here, as the stub would have, and resume the interrupted context: the thread
     * re-executes the faulting instruction and takes the fault again, this time as itself. */
    struct inject_frame* frame = (struct inject_frame*)(uintptr_t)CTX_FIRST_ARGUMENT(pointers->context_record);

    run_injected_handler(frame);
    *pointers->context_record = frame->saved;
    return EXCEPTION_CONTINUE_EXECUTION_VALUE;
  }
#endif
  sig = signal_for_exception(pointers->exception_record, &code);
  crt_signal_state* state;
  crt_signal_ucontext uc;
  siginfo_t info;
  unsigned long old_mask;

  if (sig == 0 || !signal_in_range(sig) || !action_installed[sig]) {
    return EXCEPTION_CONTINUE_SEARCH_VALUE;
  }
  state = current_state();
  old_mask = state->mask;
  fill_siginfo(&info, sig, code);
  if (pointers->exception_record->exception_code == STATUS_PRIVILEGED_INSTRUCTION_CODE) {
    info.si_addr = 0;
  } else if (sig == SIGSEGV && pointers->exception_record->number_parameters >= 2) {
    info.si_addr = (void*)(uintptr_t)pointers->exception_record->exception_information[1];
  } else {
    info.si_addr = pointers->exception_record->exception_address;
  }
  context_to_ucontext(pointers->context_record, &uc, old_mask);
#if defined(__x86_64__)
  if (pointers->exception_record->exception_code == STATUS_BREAKPOINT_CODE) {
    /* Windows reports the address of the int3 itself; Linux's SIGTRAP reports the instruction after
     * it (the trap has already executed), which is what a handler -- JavaScriptCore's -- expects, and
     * resuming at the int3 would trap again forever. */
    uc.uc_mcontext.gregs[REG_RIP] += 1;
  }
  uc.uc_mcontext.gregs[REG_CR2] = (greg_t)(uintptr_t)info.si_addr;
#else
  /* arm64's brk leaves the pc on the instruction, as Linux reports it: the handler advances it. */
  uc.uc_mcontext.fault_address = (unsigned long long)(uintptr_t)info.si_addr;
#endif
  /* A synchronous fault is delivered even if the signal is blocked (the kernel would kill the
   * process; here the handler is the best outcome), under the handler's mask. */
  state->mask = mask_for_handler(sig, old_mask);
  __crt_signal_dispatch_info(sig, &info, &uc);
  state->mask = old_mask;
  /* What arrived while the fault handler held its mask (JavaScriptCore's blocks every signal there) is
   * delivered now, as the kernel does on the way out of a signal handler: a thread-suspend request
   * sent to a thread that was inside the handler would otherwise wait for ever. It sees the context
   * the fault handler left (the thread resumes there). */
  deliver_pending_with(&uc);
  ucontext_to_context(&uc, pointers->context_record);
  return EXCEPTION_CONTINUE_EXECUTION_VALUE;
}
#endif
