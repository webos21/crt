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
#define STATUS_INT_DIVIDE_BY_ZERO_CODE 0xC0000094UL
#define STATUS_FLOAT_DIVIDE_BY_ZERO_CODE 0xC000008EUL
#define EXCEPTION_CONTINUE_EXECUTION_VALUE (-1L)
#define EXCEPTION_CONTINUE_SEARCH_VALUE 0L

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

__declspec(dllimport) DWORD CRT_WINAPI SuspendThread(HANDLE thread);
__declspec(dllimport) DWORD CRT_WINAPI ResumeThread(HANDLE thread);
__declspec(dllimport) BOOL CRT_WINAPI GetThreadContext(HANDLE thread, struct crt_win_context* context);
__declspec(dllimport) BOOL CRT_WINAPI SetThreadContext(HANDLE thread, const struct crt_win_context* context);
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
__declspec(dllimport) unsigned long long CRT_WINAPI GetTickCount64(void);
__declspec(dllimport) void CRT_WINAPI WakeByAddressAll(void* address);
__declspec(dllimport) void* CRT_WINAPI AddVectoredExceptionHandler(unsigned long first,
                                                                   long(CRT_WINAPI* handler)(struct crt_exception_pointers*));
__declspec(dllimport) HANDLE CRT_WINAPI GetModuleHandleA(const char* name);
__declspec(dllimport) void* CRT_WINAPI GetProcAddress(HANDLE module, const char* name);
__declspec(dllimport) size_t CRT_WINAPI VirtualQuery(const void* address, struct crt_memory_info* buffer, size_t length);

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
static volatile long exception_handler_added;

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

/* Makes the calling thread known to the signal machinery: a handle other threads can suspend it
 * through, and the event its sigsuspend() waits on. Idempotent. */
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
  restore_context_function();
}

unsigned long __crt_windows_signal_current_mask(void) {
  __crt_windows_signal_attach_current();
  return current_state()->mask;
}

/* The thread has ended (or is ending): nobody can signal it any more. */
void __crt_windows_signal_detach_current(void) {
  crt_signal_state* state = current_state();

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
  ucontext_t blank;

  if (context == 0) {
    /* No interrupted context to show (the signal is delivered from a call, not by interruption): an
     * SA_SIGINFO handler still gets a readable, empty ucontext_t rather than a null pointer. */
    memset(&blank, 0, sizeof(blank));
    context = &blank;
  }
  state->mask = mask_for_handler(sig, old_mask);
  __crt_signal_dispatch_info(sig, info, context);
  state->mask = old_mask;
}

/* Delivers every pending, unblocked signal to the calling thread (POSIX: on the way out of a call that
 * unblocked it). */
static void deliver_pending_with(void* context) {
  crt_signal_state* state = current_state();

  for (;;) {
    unsigned long deliverable = state->pending & ~state->mask;
    int sig;
    siginfo_t info;

    if (deliverable == 0) {
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

/* ---- the interrupted thread's frame ---- */

struct inject_frame {
  struct crt_win_context saved;
  ucontext_t uc;
  siginfo_t info;
  int sig;
  unsigned long saved_mask;
  crt_signal_state* state;
};

static void context_to_ucontext(const struct crt_win_context* from, ucontext_t* uc, unsigned long mask) {
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
static void ucontext_to_context(const ucontext_t* uc, struct crt_win_context* to) {
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

/* Runs on the interrupted thread, on the frame the sender built, called by the entry stub below.
 * Never returns: it restores the (possibly rewritten) context. */
void __crt_windows_signal_run(struct inject_frame* frame) {
  crt_signal_state* state = frame->state;
  restore_context_fn restore = restore_context_function();

  /* The sender already put the handler's mask in place (so no second signal could slip in between
   * the interruption and this point); the handler runs under it and the mask goes back afterwards. */
  __crt_signal_dispatch_info(frame->sig, &frame->info, &frame->uc);
  state->mask = frame->saved_mask;
  /* A signal that arrived (and was held back by the mask) while the handler ran sees the same
   * interrupted context, which the handler may rewrite too. */
  deliver_pending_with(&frame->uc);
  ucontext_to_context(&frame->uc, &frame->saved);
  if (restore != 0) {
    restore(&frame->saved, 0);
  }
  _exit(128 + SIGABRT); /* the context could not be restored: no way back */
}

/* The stub a suspended thread is redirected to: %rcx is the frame, %rsp is 8 mod 16 as after a call. */
__asm__(".text\n"
        ".globl __crt_windows_signal_entry\n"
        ".def __crt_windows_signal_entry; .scl 2; .type 32; .endef\n"
        ".p2align 4, 0x90\n"
        "__crt_windows_signal_entry:\n"
        "  subq $40, %rsp\n"
        "  call __crt_windows_signal_run\n"
        "  ud2\n");
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

/* Interrupts `target` (not the caller) with `sig`. Returns 1 when the handler was set up on the thread,
 * 0 when it could not be done safely now (the signal is left pending), -1 when the thread is gone. */
static int inject(crt_signal_state* target, int sig) {
  struct crt_win_context context;
  struct inject_frame* frame;
  unsigned long long frame_address;
  unsigned long old_mask;
  HANDLE handle = target->thread_handle;
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
  if (in_operating_system_code(context.rip) || (target->mask & bit_of(sig)) != 0) {
    ResumeThread(handle);
    return 0;
  }
  /* No red zone on Windows, but a margin below the stack pointer costs nothing. */
  frame_address = (context.rsp - 128 - sizeof(struct inject_frame)) & ~15ULL;
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
  context_to_ucontext(&context, &frame->uc, old_mask);
  /* The handler's mask goes in now, while the thread cannot run: nothing can be delivered to it
   * between the interruption and the handler's first instruction. */
  target->mask = mask_for_handler(sig, old_mask);
  __atomic_fetch_and(&target->pending, ~bit_of(sig), __ATOMIC_SEQ_CST);

  context.context_flags = CONTEXT_REDIRECT;
  context.rsp = frame_address - 8;
  context.rip = (unsigned long long)(uintptr_t)__crt_windows_signal_entry;
  context.rcx = (unsigned long long)(uintptr_t)frame;
  if (SetThreadContext(handle, &context)) {
    result = 1;
  } else {
    target->mask = old_mask;
    __atomic_fetch_or(&target->pending, bit_of(sig), __ATOMIC_SEQ_CST);
  }
  ResumeThread(handle);
  return result;
}

/* ---- interruptible waits (see private/crt_signal_wait.h) ---- */

int __crt_windows_signal_wait_begin(volatile void* address) {
  crt_signal_state* state = current_state();

  __crt_windows_signal_attach_current();
  state->wait_address = (void*)address;
  __atomic_store_n(&state->in_wait, 1, __ATOMIC_SEQ_CST);
  /* A sender sets `pending` and then looks at in_wait; this side sets in_wait and then looks at
   * `pending`: one of the two always sees the other, so a wake-up cannot be lost. */
  return (__atomic_load_n(&state->pending, __ATOMIC_SEQ_CST) & ~state->mask) != 0;
}

int __crt_windows_signal_wait_end(void) {
  crt_signal_state* state = current_state();
  int ran = 0;

  __atomic_store_n(&state->in_wait, 0, __ATOMIC_SEQ_CST);
  if ((state->pending & ~state->mask) != 0) {
    ucontext_t context;

    getcontext(&context);
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
  unsigned long long deadline = milliseconds == 0xffffffffUL ? 0 : GetTickCount64() + milliseconds;

  for (;;) {
    HANDLE handles[2];
    DWORD count = 1;
    DWORD remaining = 0xffffffffUL;
    DWORD result;
    int blocked;

    if (deadline != 0) {
      unsigned long long now = GetTickCount64();

      remaining = now >= deadline ? 0 : (DWORD)(deadline - now);
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
    if (deadline != 0 && GetTickCount64() >= deadline) {
      return WAIT_TIMEOUT_CODE;
    }
  }
}

int __crt_windows_signal_sleep(unsigned long milliseconds) {
  unsigned long long deadline = GetTickCount64() + milliseconds;

  for (;;) {
    unsigned long long now = GetTickCount64();
    int blocked;
    int ran;

    if (now >= deadline) {
      return 0;
    }
    blocked = __crt_windows_signal_wait_begin(0);
    if (!blocked) {
      WaitForSingleObject(__crt_windows_signal_wake_event(), (DWORD)(deadline - now));
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

static long CRT_WINAPI exception_handler(struct crt_exception_pointers* pointers);

int __crt_signal_backend_set_action_ex(int bionic_sig, enum crt_signal_backend_action action,
                                       int flags, const sigset_t* mask) {
  if (signal_in_range(bionic_sig)) {
    action_flags[bionic_sig] = flags;
    action_mask[bionic_sig] = mask != 0 ? (unsigned long)*mask : 0UL;
    action_installed[bionic_sig] = action == CRT_SIGNAL_BACKEND_DISPATCH;
    if (action == CRT_SIGNAL_BACKEND_DISPATCH &&
        (bionic_sig == SIGTRAP || bionic_sig == SIGSEGV || bionic_sig == SIGILL || bionic_sig == SIGFPE) &&
        __atomic_exchange_n(&exception_handler_added, 1, __ATOMIC_SEQ_CST) == 0) {
      AddVectoredExceptionHandler(1, exception_handler);
    }
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

  __crt_windows_signal_attach_current();
  state->mask = (unsigned long)*mask & ~(bit_of(SIGKILL) | bit_of(SIGSTOP));
  for (;;) {
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
  result = inject(target, sig);
  return result < 0 ? ESRCH : 0;
}

crt_signal_state* __crt_windows_signal_state_of_initial_thread(void) {
  return initial_state;
}

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

static long CRT_WINAPI exception_handler(struct crt_exception_pointers* pointers) {
  int code = 0;
  int sig = signal_for_exception(pointers->exception_record, &code);
  crt_signal_state* state;
  ucontext_t uc;
  siginfo_t info;
  unsigned long old_mask;

  if (sig == 0 || !signal_in_range(sig) || !action_installed[sig]) {
    return EXCEPTION_CONTINUE_SEARCH_VALUE;
  }
  state = current_state();
  old_mask = state->mask;
  fill_siginfo(&info, sig, code);
  if (sig == SIGSEGV && pointers->exception_record->number_parameters >= 2) {
    info.si_addr = (void*)(uintptr_t)pointers->exception_record->exception_information[1];
  } else {
    info.si_addr = pointers->exception_record->exception_address;
  }
  context_to_ucontext(pointers->context_record, &uc, old_mask);
  if (pointers->exception_record->exception_code == STATUS_BREAKPOINT_CODE) {
    /* Windows reports the address of the int3 itself; Linux's SIGTRAP reports the instruction after
     * it (the trap has already executed), which is what a handler -- JavaScriptCore's -- expects, and
     * resuming at the int3 would trap again forever. */
    uc.uc_mcontext.gregs[REG_RIP] += 1;
  }
  uc.uc_mcontext.gregs[REG_CR2] = (greg_t)(uintptr_t)info.si_addr;
  /* A synchronous fault is delivered even if the signal is blocked (the kernel would kill the
   * process; here the handler is the best outcome), under the handler's mask. */
  state->mask = mask_for_handler(sig, old_mask);
  __crt_signal_dispatch_info(sig, &info, &uc);
  state->mask = old_mask;
  ucontext_to_context(&uc, pointers->context_record);
  return EXCEPTION_CONTINUE_EXECUTION_VALUE;
}
