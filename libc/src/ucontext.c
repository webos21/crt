#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

/*
 * makecontext() is the one piece of this file not implemented in
 * assembly (get/set/swapcontext.S, one set per host/arch, mirror this
 * project's already-verified setjmp.S/longjmp.S register sets -- see
 * ucontext.h's own top comment). This function only ever needs to poke
 * a few known byte offsets into the target ucontext_t's opaque
 * mcontext_t and build a small heap "bootstrap frame" the trampoline
 * (__crt_makecontext_trampoline, in the matching .S file) reads on that
 * context's first activation.
 *
 * Deliberately never freed: the bootstrap frame must stay alive from
 * makecontext() until the context is entered (via setcontext()/
 * swapcontext()) *and* its function returns (so uc_link can still be
 * read afterward) -- an unknown, caller-controlled amount of time later.
 * One small (56-byte) leak per makecontext() call is a documented,
 * deliberate simplification for a feature with no identified near-term
 * consumer, not an oversight.
 */
/*
 * Every field here must be exactly 8 bytes: the trampoline assembly
 * (__crt_makecontext_trampoline in each host/arch's ucontext.S) indexes
 * into this struct by fixed byte offset (0/8/16/24/32/40/48), so this
 * layout has to hold on every host -- including Windows, where plain
 * `long`/`unsigned long` are only 4 bytes (LLP64), unlike Linux/macOS's
 * LP64 (where they're already 8). int64_t/uint64_t side-step that
 * platform difference instead of silently relying on it.
 */
struct crt_makecontext_frame {
  void (*func)(void);
  int64_t argc;
  uint64_t argv[4];
  ucontext_t* link;
};

extern void __crt_makecontext_trampoline(void);

#if defined(CRT_TARGET_OS_WINDOWS) && (defined(__x86_64__) || defined(_M_X64))
#define CRT_MCTX_BOOTSTRAP_OFFSET 0
#define CRT_MCTX_SP_OFFSET 64
#define CRT_MCTX_PC_OFFSET 72
#define CRT_MCTX_SP_NEEDS_ODD_ALIGN 1
#elif defined(__x86_64__) || defined(_M_X64)
#define CRT_MCTX_BOOTSTRAP_OFFSET 0
#define CRT_MCTX_SP_OFFSET 48
#define CRT_MCTX_PC_OFFSET 56
#define CRT_MCTX_SP_NEEDS_ODD_ALIGN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CRT_MCTX_BOOTSTRAP_OFFSET 0
#define CRT_MCTX_SP_OFFSET 96
#define CRT_MCTX_PC_OFFSET 88
#define CRT_MCTX_SP_NEEDS_ODD_ALIGN 0
#else
#error "ucontext.c: unsupported architecture"
#endif

static void mctx_store_ptr(mcontext_t* mc, size_t byte_offset, void* value) {
  memcpy(mc->__opaque + byte_offset, &value, sizeof(value));
}

void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...) {
  struct crt_makecontext_frame* frame;
  va_list ap;
  int i;
  unsigned char* top;
  uintptr_t aligned_sp;

  if (ucp == 0 || func == 0) {
    return;
  }
  frame = (struct crt_makecontext_frame*)calloc(1, sizeof(*frame));
  if (frame == 0) {
    return; /* POSIX defines no error-reporting contract for makecontext() */
  }
  frame->func = func;
  frame->argc = argc;
  frame->link = ucp->uc_link;

  /*
   * Read as a fixed 8-byte type rather than the `int` the POSIX
   * prototype suggests, matching glibc's own real x86_64 makecontext()
   * behavior -- on every 64-bit ABI this project targets, `int`-sized
   * varargs would silently truncate any pointer-valued argument callers
   * commonly pass in practice, so pointer-width is the only honest
   * choice here despite the letter of the prototype. int64_t rather
   * than `long` for the same LLP64-vs-LP64 reason as the struct above.
   */
  va_start(ap, argc);
  for (i = 0; i < argc && i < 4; ++i) {
    frame->argv[i] = (uint64_t)va_arg(ap, int64_t);
  }
  va_end(ap);

  top = (unsigned char*)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;
  aligned_sp = (uintptr_t)top & ~(uintptr_t)15;
#if CRT_MCTX_SP_NEEDS_ODD_ALIGN
  /*
   * x86_64 (both SysV and Windows): the trampoline's own `call` needs
   * RSP % 16 == 8 at the trampoline's entry point, not 0 -- see this
   * project's ucontext.S files' own comments for the full derivation
   * (SysV wants RSP%16==0 right before `call`; Windows wants it ==0
   * right after, which its extra 32-byte-aligned shadow-space `sub`
   * preserves the parity through -- both end up needing the same -8
   * adjustment here). aarch64 has no such off-by-8 quirk: AAPCS64's
   * `bl`/`blr` never touch SP, so a plain 16-aligned SP is correct.
   */
  aligned_sp -= 8;
#endif

  mctx_store_ptr(&ucp->uc_mcontext, CRT_MCTX_BOOTSTRAP_OFFSET, frame);
  mctx_store_ptr(&ucp->uc_mcontext, CRT_MCTX_SP_OFFSET, (void*)aligned_sp);
  mctx_store_ptr(&ucp->uc_mcontext, CRT_MCTX_PC_OFFSET, (void*)__crt_makecontext_trampoline);

#if defined(CRT_TARGET_OS_WINDOWS) && (defined(__x86_64__) || defined(_M_X64))
  /*
   * Seed this context's TEB NT_TIB.StackBase/StackLimit/DeallocationStack
   * (mcontext offsets 240/248/256 -- see ucontext.S's own comment for why
   * Windows specifically needs this and Linux/macOS don't) for its first
   * activation, since there is no earlier getcontext() call on *this*
   * stack to have captured them from. setcontext()/swapcontext() install
   * these into the real TEB the moment this context actually starts
   * running.
   */
  mctx_store_ptr(&ucp->uc_mcontext, 240, top);
  mctx_store_ptr(&ucp->uc_mcontext, 248, ucp->uc_stack.ss_sp);
  mctx_store_ptr(&ucp->uc_mcontext, 256, ucp->uc_stack.ss_sp);
#endif
}
