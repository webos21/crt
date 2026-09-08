/* getcontext()/setcontext()/makecontext()/swapcontext() -- real on every
 * host this project targets (see ucontext.h's own top comment for why
 * this is unlike most of the other "lower priority" gaps this session
 * closed: no host-level reason exists to hold any of Linux/macOS/Windows
 * back here). This project's own per-arch assembly mirrors its already-
 * verified setjmp.S/longjmp.S register sets; verified directly on
 * Windows x86_64 in this session (including a real coroutine round trip
 * that caught and fixed a genuine swapcontext() resume-point bug -- see
 * ucontext.S's own comment), Linux/macOS and aarch64 reasoned carefully
 * from the same proven setjmp.S register sets but not yet run on real
 * hardware from this session. */
#include <stdio.h>
#include <string.h>
#include <ucontext.h>

static int fail(const char* message) {
  fprintf(stderr, "ucontext_test: %s\n", message);
  return 1;
}

static ucontext_t probe_ctx;
static ucontext_t main_ctx;
static ucontext_t coro_ctx;
static unsigned char coro_stack[65536];
static int coro_state;
static int coro_arg1;
static int coro_arg2;

static void coro_entry(int a, int b) {
  coro_state = 1;
  coro_arg1 = a;
  coro_arg2 = b;
  swapcontext(&coro_ctx, &main_ctx); /* yield back to main once */
  coro_state = 2;
  /* falling off the end here hands control to uc_link (main_ctx). */
}

int main(void) {
  int resume_count = 0;

  /* Isolated check: getcontext() returns 0 on a plain, never-resumed
   * call. probe_ctx is never handed to setcontext()/swapcontext(), so
   * there is no resume-point hazard in checking its return value here. */
  memset(&probe_ctx, 0, sizeof(probe_ctx));
  if (getcontext(&probe_ctx) != 0) {
    return fail("getcontext basic return value");
  }

  /* --- getcontext()/setcontext(): a setjmp/longjmp-shaped round trip.
   * POSIX getcontext() has no setjmp()-style "was this a live call or a
   * resume" signal -- unlike this file's swapcontext() calls below
   * (whose resume path is this project's own asm explicitly zeroing the
   * return register before returning), a setcontext()-driven resume
   * here lands right after the call instruction with an unspecified
   * value in the return-value register. So this call's return value is
   * deliberately never inspected -- only the separately tracked
   * resume_count flag decides what happened. */
  memset(&main_ctx, 0, sizeof(main_ctx));
  getcontext(&main_ctx);
  ++resume_count;
  if (resume_count == 1) {
    setcontext(&main_ctx);
    return fail("setcontext did not resume"); /* unreachable on success */
  }
  if (resume_count != 2) {
    return fail("setcontext resume count");
  }

  /* --- makecontext()/swapcontext(): a real coroutine round trip,
   * including argument passing and uc_link on normal return. */
  memset(&coro_ctx, 0, sizeof(coro_ctx));
  if (getcontext(&coro_ctx) != 0) {
    return fail("getcontext coro");
  }
  coro_ctx.uc_stack.ss_sp = coro_stack;
  coro_ctx.uc_stack.ss_size = sizeof(coro_stack);
  coro_ctx.uc_link = &main_ctx;
  makecontext(&coro_ctx, (void (*)(void))coro_entry, 2, 11, 22);

  coro_state = 0;
  if (swapcontext(&main_ctx, &coro_ctx) != 0) {
    return fail("swapcontext to coro (first entry)");
  }
  if (coro_state != 1 || coro_arg1 != 11 || coro_arg2 != 22) {
    return fail("coro first run / makecontext argument passing");
  }

  /* Resume the coroutine past its own yield point; it runs to
   * completion and uc_link's setcontext(&main_ctx) lands right back
   * here, since this swapcontext() call is what captured main_ctx this
   * time. */
  if (swapcontext(&main_ctx, &coro_ctx) != 0) {
    return fail("swapcontext to coro (resume)");
  }
  if (coro_state != 2) {
    return fail("coro second run (post-yield)");
  }

  printf("ucontext_test: ok\n");
  return 0;
}
