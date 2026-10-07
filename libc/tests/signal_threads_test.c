/* Per-thread signal semantics a multi-threaded runtime (WTF/JavaScriptCore's thread suspend and
 * VM traps) depends on (Linux, macOS/arm64 and, since the Windows signal gate, Windows/x64): sigsuspend really blocks until a signal's handler has run,
 * the handler's sa_mask is applied while it runs (a second signal stays pending until the
 * handler returns), and the signal mask is per thread. CRT used to keep one software mask for
 * the process, ignore sa_mask and flags, and implement sigsuspend as a call that returned
 * immediately -- which let a thread another thread believed suspended keep running. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Real kernel signals reach CRT handlers on Linux and, since the Web Tranche 1 macOS replay, on
 * Apple Silicon (libc/src/arch/macos/common/signal_backend.c). macOS x86_64 still keeps the software
 * mask and a stub sigsuspend. */
#if defined(__linux__) || (defined(__APPLE__) && defined(__aarch64__)) ||     defined(CRT_TARGET_OS_WINDOWS)
#define HAVE_REAL_SIGNALS 1
#endif

#if defined(HAVE_REAL_SIGNALS)
static int failures;

#define CHECK(condition, message)                                                   \
  do {                                                                              \
    if (!(condition)) {                                                             \
      fprintf(stderr, "signal_threads_test: %s (line %d)\n", message, __LINE__);    \
      ++failures;                                                                   \
    }                                                                               \
  } while (0)

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static volatile int usr1_handled;
static volatile int usr2_handled;
static volatile int order[4];
static volatile int order_count;

static void record(int value) {
  if (order_count < 4) order[order_count++] = value;
}

static void on_usr1(int sig) {
  (void)sig;
  usr1_handled = 1;
  record(1);
  /* Another thread sends SIGUSR2 while this handler runs; sa_mask must hold it back. */
  for (int i = 0; i < 100 && !usr2_handled; ++i) usleep(2000);
  record(usr2_handled ? 20 : 10); /* 20 would mean SIGUSR2 ran inside the handler. */
}

static void on_usr2(int sig) {
  (void)sig;
  usr2_handled = 1;
  record(2);
}

static volatile long sleeper_tid;
static volatile int sleeper_woke;
static volatile double sleeper_elapsed;

static void* sleeper(void* argument) {
  sigset_t block, wait_mask;
  (void)argument;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &block, 0);
  sleeper_tid = 1;
  sigfillset(&wait_mask);
  sigdelset(&wait_mask, SIGUSR1);
  double started = now();
  int result = sigsuspend(&wait_mask);
  sleeper_elapsed = now() - started;
  sleeper_woke = (result == -1 && errno == EINTR);
  return 0;
}

static void test_sigsuspend_blocks(void) {
  struct sigaction action;
  pthread_t thread;
  int i;

  memset(&action, 0, sizeof(action));
  action.sa_handler = on_usr1;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGUSR2);
  sigaction(SIGUSR1, &action, 0);
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_usr2;
  sigaction(SIGUSR2, &action, 0);

  pthread_create(&thread, 0, sleeper, 0);
  for (i = 0; i < 1000 && !sleeper_tid; ++i) usleep(1000);
  usleep(150000); /* the sleeper is now parked in sigsuspend */
  CHECK(!sleeper_woke, "sigsuspend returned before any signal arrived");
  pthread_kill(thread, SIGUSR1);
  for (i = 0; i < 300 && !usr1_handled; ++i) usleep(2000);
  CHECK(usr1_handled, "the SIGUSR1 handler ran on the suspended thread");
  /* The handler is now polling for SIGUSR2; send it and require it to wait. */
  pthread_kill(thread, SIGUSR2);
  pthread_join(thread, 0);
  CHECK(sleeper_woke, "sigsuspend returned -1/EINTR after the handler ran");
  CHECK(sleeper_elapsed >= 0.12, "sigsuspend actually waited (~150 ms)");
  CHECK(order_count == 3 && order[0] == 1 && order[1] == 10 && order[2] == 2,
        "SIGUSR2 stayed pending (sa_mask) until the SIGUSR1 handler returned");
}

static volatile int child_blocked, child_after;

static void* mask_child(void* argument) {
  sigset_t set, unblock;
  (void)argument;
  sigemptyset(&set);
  sigprocmask(SIG_BLOCK, 0, &set);
  child_blocked = sigismember(&set, SIGUSR2);
  sigemptyset(&unblock);
  sigaddset(&unblock, SIGUSR2);
  pthread_sigmask(SIG_UNBLOCK, &unblock, 0);
  sigemptyset(&set);
  sigprocmask(SIG_BLOCK, 0, &set);
  child_after = sigismember(&set, SIGUSR2);
  return 0;
}

static void test_mask_is_per_thread(void) {
  sigset_t block, current;
  pthread_t thread;

  sigemptyset(&block);
  sigaddset(&block, SIGUSR2);
  sigemptyset(&current);
  pthread_sigmask(SIG_BLOCK, &block, 0);
  sigprocmask(SIG_BLOCK, 0, &current);
  CHECK(sigismember(&current, SIGUSR2) == 1, "the calling thread's mask shows the blocked signal");
  /* A new thread inherits the creator's mask and has its own from then on. */
  pthread_create(&thread, 0, mask_child, 0);
  pthread_join(thread, 0);
  CHECK(child_blocked == 1, "a new thread inherits the creator's mask");
  CHECK(child_after == 0, "unblocking in the child works");
  sigemptyset(&current);
  sigprocmask(SIG_BLOCK, 0, &current);
  CHECK(sigismember(&current, SIGUSR2) == 1, "the child's change did not touch this thread's mask");
  pthread_sigmask(SIG_UNBLOCK, &block, 0);
}

/* WTF's thread suspend/resume, in miniature: SIGUSR1 parks the target in its handler (sigsuspend with
 * only SIGUSR1 open), a second SIGUSR1 -- sent with pthread_kill(), which needs pthread_self() and the
 * thread registry -- releases it. The target spends its whole life in errno/pthread_self/
 * pthread_getspecific, so the signal lands inside the registry's lookup path almost every time.
 * The registry used to be one global spin lock: the parked thread then held it and the thread sent
 * to resume it spun on it forever (found by the JavaScriptCore WebAssembly acceptance). */
static volatile int hot_stop;
static volatile int hot_started;
static volatile int park_phase;
static volatile int parked, released;

static void park_handler(int sig) {
  (void)sig;
  if (park_phase == 0) {
    sigset_t open_mask;
    park_phase = 1;
    parked = 1;
    sigfillset(&open_mask);
    sigdelset(&open_mask, SIGUSR1);
    sigsuspend(&open_mask);
    park_phase = 0;
    released = 1;
  }
}

static void* hot_thread(void* argument) {
  unsigned long spins = 0;
  (void)argument;
  hot_started = 1;
  while (!hot_stop) {
    errno = (int)(spins & 7);
    spins += (unsigned long)(errno + (pthread_self() != 0) + (pthread_getspecific(0) == 0));
  }
  return (void*)(uintptr_t)spins;
}

static int wait_for(volatile int* flag, double seconds) {
  double deadline = now() + seconds;
  while (!*flag) {
    if (now() > deadline) {
      return 0;
    }
    sched_yield();
  }
  return 1;
}

static void test_suspend_resume_in_hot_thread(void) {
  struct sigaction action;
  pthread_t thread;
  int round;
  int stuck = 0;

  memset(&action, 0, sizeof action);
  action.sa_handler = park_handler;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGUSR1);
  sigaction(SIGUSR1, &action, 0);
  pthread_create(&thread, 0, hot_thread, 0);
  /* A new thread joins the registry as it starts; pthread_kill() before that cannot find it
   * (a documented gap, docs/signal_delivery.md), and the runtimes that use this protocol only
   * signal threads that have finished starting. */
  wait_for(&hot_started, 10.0);
  for (round = 0; round < 3000 && !stuck; ++round) {
    parked = 0;
    released = 0;
    pthread_kill(thread, SIGUSR1);
    if (!wait_for(&parked, 10.0)) {
      fprintf(stderr, "signal_threads_test: the target never parked\n");
      stuck = 1;
      break;
    }
    pthread_kill(thread, SIGUSR1);
    if (!wait_for(&released, 10.0)) {
      fprintf(stderr, "signal_threads_test: the parked target was never released\n");
      stuck = 1;
    }
  }
  CHECK(!stuck, "a thread parked in a signal handler could not be resumed (registry lock held by the parked thread?)");
  if (stuck) {
    fprintf(stderr, "signal_threads_test: stuck at round %d\n", round);
    _exit(1);
  }
  hot_stop = 1;
  pthread_join(thread, 0);
}
#endif

int main(void) {
#if defined(HAVE_REAL_SIGNALS)
  test_sigsuspend_blocks();
  test_mask_is_per_thread();
  test_suspend_resume_in_hot_thread();
  if (failures != 0) {
    fprintf(stderr, "signal_threads_test: %d check(s) failed\n", failures);
    return 1;
  }
#endif
  printf("signal_threads_test: ok\n");
  return 0;
}
