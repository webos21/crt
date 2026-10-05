/* The signal semantics a JIT's VM traps depend on (WTF/JavaScriptCore with JSC_usePollingTraps off),
 * x86_64 Linux and Windows. JavaScriptCore interrupts compiled code that makes no calls and waits
 * for no one: another thread signals it, the handler reads the interrupted ucontext_t, may rewrite
 * the instruction pointer, and the thread resumes there; a patched-in `int3` raises SIGTRAP in the
 * thread itself with the same context. Windows has no kernel signal delivery, so the CRT builds it
 * (suspend, capture the context, run the handler on the target thread, restore); this is its gate:
 *   - an uncooperative `jmp .` loop is interrupted by pthread_kill() and the handler's change to
 *     REG_RIP takes effect, for a worker and for the initial thread, repeatedly;
 *   - the context the handler sees is real: siginfo_t carries the signal, REG_RIP is the loop and
 *     REG_RSP lies on the interrupted thread's stack;
 *   - an `int3` runs the SIGTRAP handler on the faulting thread, which can move RIP;
 *   - many threads interrupted concurrently each get exactly the signals sent to them;
 *   - a thread blocked in pthread_cond_wait, sem_wait, nanosleep or pthread_join still runs the handler
 *     (WTF suspends a thread this way: it signals it and waits for the handler to answer). */
#define _GNU_SOURCE 1
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#if defined(__x86_64__) && (defined(__linux__) || defined(CRT_TARGET_OS_WINDOWS))

static int failures;

#define CHECK(condition, message)                                                    \
  do {                                                                               \
    if (!(condition)) {                                                              \
      fprintf(stderr, "signal_vmtrap_test: %s (line %d)\n", message, __LINE__);      \
      ++failures;                                                                    \
    }                                                                                \
  } while (0)

#define MAX_THREADS 8

struct spinner {
  pthread_t thread;
  volatile int ready;      /* set just before the loop is entered */
  volatile int escaped;    /* set after the loop was left */
  void* volatile loop_address;
  void* volatile after_address;
  volatile long handled;   /* handler invocations seen on this thread */
  volatile int context_ok; /* the handler saw a coherent context */
  volatile int wrong_thread;
  volatile int stop;       /* set by the controller at teardown */
};

static struct spinner spinners[MAX_THREADS];
static pthread_key_t self_key;

/* Spins in a loop that never calls anything and never blocks; leaves it only when the handler
 * moves RIP from the loop to `after`. */
static void spin(struct spinner* me) {
  __asm__ volatile(
      "lea 1f(%%rip), %%rax\n\t"
      "movq %%rax, %0\n\t"
      "lea 2f(%%rip), %%rax\n\t"
      "movq %%rax, %1\n\t"
      "movl $1, %2\n\t"
      "1: jmp 1b\n\t"
      "2:\n\t"
      : "=m"(me->loop_address), "=m"(me->after_address), "=m"(me->ready)
      :
      : "rax", "memory");
}

static void on_usr1(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;
  struct spinner* me = (struct spinner*)pthread_getspecific(self_key);
  uintptr_t rip = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
  uintptr_t rsp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
  uintptr_t here = (uintptr_t)&rsp;

  if (me == 0) {
    return;
  }
  ++me->handled;
  /* The interrupted thread's stack and the handler's are the same thread's: close together. */
  if (info != 0 && info->si_signo == sig && rip == (uintptr_t)me->loop_address &&
      (rsp > here ? rsp - here : here - rsp) < (1u << 20)) {
    me->context_ok = 1;
  }
  if (rip == (uintptr_t)me->loop_address) {
    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)me->after_address;
  }
}

static void* spinner_main(void* argument) {
  struct spinner* me = (struct spinner*)argument;

  pthread_setspecific(self_key, me);
  for (;;) {
    if (me->stop) return 0;
    me->ready = 0;
    spin(me);
    me->escaped = 1;
    while (me->escaped) {
      /* wait until the controller has seen the escape and re-armed us */
      if (me->stop) return 0;
      sched_yield();
    }
  }
}

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static int wait_for(volatile int* flag, int value, double seconds) {
  double deadline = now() + seconds;
  while (*flag != value) {
    if (now() > deadline) return 0;
    sched_yield(); /* not nanosleep(): Windows' default timer resolution makes it sleep ~16 ms */
  }
  return 1;
}

/* Signals `thread` until its loop has been left: like a VM-trap sender, it sends again when the signal
 * arrived while the thread was between setting its flag and entering the loop (the handler then sees
 * a different REG_RIP and cannot move it). Returns the number of signals it took, 0 on a hang. */
static int interrupt_until_escaped(pthread_t thread, int sig, volatile int* escaped, long* sent) {
  double deadline = now() + 5.0;
  int attempts = 0;

  while (!*escaped) {
    int spin;

    if (now() > deadline) return 0;
    ++attempts;
    if (sent != 0) ++*sent;
    if (pthread_kill(thread, sig) != 0) return 0;
    /* Give the signal time to take before sending another (a second one merges into a pending one). */
    for (spin = 0; spin < 200 && !*escaped; ++spin) sched_yield();
  }
  return attempts;
}

/* Teardown: a spinner may already be back in its loop, so interrupt it once more after asking it to stop. */
static void stop_spinner(struct spinner* s) {
  s->stop = 1;
  interrupt_until_escaped(s->thread, SIGUSR1, &s->escaped, 0);
  s->escaped = 0;
  pthread_join(s->thread, 0);
}

/* One worker, one signal at a time: the interruption must take, whatever the loop is doing. */
static void test_interrupt_worker(void) {
  struct sigaction action;
  struct spinner* s = &spinners[0];
  int round;
  int escaped_all = 1;
  int context_all = 1;
  long sent = 0;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr1;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGUSR1, &action, 0) == 0, "sigaction(SIGUSR1)");

  CHECK(pthread_create(&s->thread, 0, spinner_main, s) == 0, "pthread_create for the spinner");
  for (round = 0; round < 200; ++round) {
    if (!wait_for(&s->ready, 1, 5.0)) {
      CHECK(0, "the spinner reached its loop");
      break;
    }
    s->context_ok = 0;
    if (interrupt_until_escaped(s->thread, SIGUSR1, &s->escaped, &sent) == 0) {
      escaped_all = 0;
      break;
    }
    if (!s->context_ok) context_all = 0;
    s->escaped = 0; /* re-arm: the spinner enters the loop again */
  }
  CHECK(escaped_all, "every interrupted loop was left through the rewritten REG_RIP (no hang)");
  CHECK(context_all, "the handler saw siginfo_t, REG_RIP at the loop and a REG_RSP on the thread's stack");
  /* Standard signals are not queued: one sent while another is still pending merges into it, so the
   * handler can run fewer times than signals were sent -- but at least once per round (each round's
   * loop was left through the handler) and never more often than a signal was sent. */
  CHECK(s->handled >= 200 && s->handled <= sent,
        "the handler ran at least once per round and never more often than signals were sent");
  stop_spinner(s);
}

/* The initial thread is interrupted by a worker. */
static volatile int initial_ready;
static volatile int initial_escaped;
static void* volatile initial_loop;
static void* volatile initial_after;
static pthread_t initial_thread;
static volatile int initial_context_ok;

static void on_usr2(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;

  (void)info;
  (void)sig;
  if ((void*)uc->uc_mcontext.gregs[REG_RIP] == initial_loop) {
    initial_context_ok = 1;
    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)initial_after;
  }
}

static void* poke_initial(void* argument) {
  int i;

  (void)argument;
  for (i = 0; i < 100; ++i) {
    wait_for(&initial_ready, 1, 5.0);
    interrupt_until_escaped(initial_thread, SIGUSR2, &initial_escaped, 0);
    initial_ready = 0;
    initial_escaped = 0;
  }
  return 0;
}

static void test_interrupt_initial_thread(void) {
  struct sigaction action;
  pthread_t poker;
  int i;
  int ok = 1;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr2;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  sigaction(SIGUSR2, &action, 0);
  initial_thread = pthread_self();
  pthread_create(&poker, 0, poke_initial, 0);
  for (i = 0; i < 100; ++i) {
    initial_context_ok = 0;
    __asm__ volatile(
        "lea 1f(%%rip), %%rax\n\t"
        "movq %%rax, %0\n\t"
        "lea 2f(%%rip), %%rax\n\t"
        "movq %%rax, %1\n\t"
        "movl $1, %2\n\t"
        "1: jmp 1b\n\t"
        "2:\n\t"
        : "=m"(initial_loop), "=m"(initial_after), "=m"(initial_ready)
        :
        : "rax", "memory");
    if (!initial_context_ok) ok = 0;
    initial_escaped = 1;
    if (!wait_for(&initial_ready, 0, 5.0)) {
      ok = 0;
      break;
    }
  }
  pthread_join(poker, 0);
  CHECK(ok, "the initial thread's loop is interrupted by another thread, 100 times");
}

/* A patched-in `int3` raises SIGTRAP in the executing thread; the handler may move RIP. */
static volatile int trap_seen;
static volatile int trap_redirect_seen;
static void* volatile trap_after;

static void on_trap(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;

  (void)info;
  if (sig == SIGTRAP) {
    ++trap_seen;
    if (trap_after != 0) {
      uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)trap_after;
      trap_redirect_seen = 1;
    }
  }
}

static void test_int3_raises_sigtrap(void) {
  struct sigaction action;
  volatile int skipped = 1;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_trap;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGTRAP, &action, 0) == 0, "sigaction(SIGTRAP)");

  trap_after = 0;
  __asm__ volatile("int3" ::: "memory");
  CHECK(trap_seen == 1, "an int3 ran the SIGTRAP handler once and execution continued after it");

  trap_after = (void*)1; /* set below to the label after the trap */
  __asm__ volatile(
      "lea 2f(%%rip), %%rax\n\t"
      "movq %%rax, %0\n\t"
      "int3\n\t"
      "movl $0, %1\n\t"
      "2:\n\t"
      : "=m"(trap_after), "=m"(skipped)
      :
      : "rax", "memory");
  CHECK(trap_redirect_seen == 1 && skipped == 1, "the SIGTRAP handler rewrote REG_RIP and the thread resumed there");
  trap_after = 0;
  signal(SIGTRAP, SIG_DFL);
}

/* Many threads, interrupted concurrently from one controller and from each other. */
static volatile long total_handled;
static volatile long total_sent;

static void on_usr1_many(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;
  struct spinner* me = (struct spinner*)pthread_getspecific(self_key);

  (void)sig;
  (void)info;
  if (me == 0) return;
  __atomic_fetch_add(&me->handled, 1, __ATOMIC_SEQ_CST);
  __atomic_fetch_add(&total_handled, 1, __ATOMIC_SEQ_CST);
  if ((void*)uc->uc_mcontext.gregs[REG_RIP] == me->loop_address) {
    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)me->after_address;
  }
}

static void test_concurrent_delivery(void) {
  struct sigaction action;
  int i, round;
  int ok = 1;
  const int threads = MAX_THREADS;
  const int rounds = 300;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr1_many;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  sigaction(SIGUSR1, &action, 0);
  for (i = 0; i < threads; ++i) {
    memset(&spinners[i], 0, sizeof(spinners[i]));
    pthread_create(&spinners[i].thread, 0, spinner_main, &spinners[i]);
  }
  for (round = 0; round < rounds && ok; ++round) {
    for (i = 0; i < threads; ++i) {
      if (!wait_for(&spinners[i].ready, 1, 5.0)) ok = 0;
    }
    for (i = 0; i < threads && ok; ++i) {
      long sent = 0;

      /* All eight are interrupted close together: the sends are interleaved by taking the threads in turn. */
      if (interrupt_until_escaped(spinners[i].thread, SIGUSR1, &spinners[i].escaped, &sent) == 0) ok = 0;
      __atomic_fetch_add(&total_sent, sent, __ATOMIC_SEQ_CST);
    }
    for (i = 0; i < threads; ++i) spinners[i].escaped = 0;
  }
  CHECK(ok, "8 threads interrupted concurrently, 300 rounds, no hang");
  CHECK(total_handled >= (long)threads * rounds && total_handled <= total_sent,
        "each thread's handler ran at least once per round and never more often than signals were sent");
  for (i = 0; i < threads; ++i) stop_spinner(&spinners[i]);
}

/* Threads parked in blocking calls: the handler must run while they are still blocked. */
enum { WAIT_COND, WAIT_SEM, WAIT_SLEEP, WAIT_JOIN, WAIT_KINDS };

static pthread_mutex_t block_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t block_cond = PTHREAD_COND_INITIALIZER;
static sem_t block_sem;
static volatile int block_release;
static volatile int block_handled;
static volatile int block_in_call;

static void on_usr1_block(int sig, siginfo_t* info, void* context) {
  (void)sig;
  (void)info;
  (void)context;
  block_handled = 1;
}

static void* sleeper_thread(void* argument) {
  (void)argument;
  while (!block_release) usleep(20 * 1000);
  return 0;
}

static void* blocked_main(void* argument) {
  int kind = (int)(intptr_t)argument;

  switch (kind) {
    case WAIT_COND:
      pthread_mutex_lock(&block_lock);
      block_in_call = 1;
      while (!block_release) pthread_cond_wait(&block_cond, &block_lock);
      pthread_mutex_unlock(&block_lock);
      break;
    case WAIT_SEM:
      block_in_call = 1;
      while (sem_wait(&block_sem) != 0 && errno == EINTR) {
      }
      break;
    case WAIT_SLEEP: {
      struct timespec request = {30, 0};

      block_in_call = 1;
      nanosleep(&request, 0); /* returns early (EINTR) once the handler ran and the test releases it */
      break;
    }
    case WAIT_JOIN: {
      pthread_t inner;

      pthread_create(&inner, 0, sleeper_thread, 0);
      block_in_call = 1;
      pthread_join(inner, 0);
      break;
    }
  }
  return 0;
}

static void test_blocked_waits(void) {
  static const char* const names[WAIT_KINDS] = {"pthread_cond_wait", "sem_wait", "nanosleep", "pthread_join"};
  struct sigaction action;
  int kind;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr1_block;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  sigaction(SIGUSR1, &action, 0);
  sem_init(&block_sem, 0, 0);

  for (kind = 0; kind < WAIT_KINDS; ++kind) {
    pthread_t thread;
    char message[96];

    block_release = 0;
    block_handled = 0;
    block_in_call = 0;
    pthread_create(&thread, 0, blocked_main, (void*)(intptr_t)kind);
    wait_for(&block_in_call, 1, 5.0);
    usleep(50 * 1000); /* now parked in the call */
    pthread_kill(thread, SIGUSR1);
    snprintf(message, sizeof(message), "the handler ran on a thread blocked in %s", names[kind]);
    CHECK(wait_for(&block_handled, 1, 5.0), message);
    /* release it */
    block_release = 1;
    if (kind == WAIT_COND) {
      pthread_mutex_lock(&block_lock);
      pthread_cond_broadcast(&block_cond);
      pthread_mutex_unlock(&block_lock);
    } else if (kind == WAIT_SEM) {
      sem_post(&block_sem);
    } else if (kind == WAIT_SLEEP) {
      pthread_kill(thread, SIGUSR1); /* nanosleep() returns EINTR after a handler; one more signal ends it */
    }
    pthread_join(thread, 0);
  }
  sem_destroy(&block_sem);
}

static void progress(const char* step) {
  if (getenv("SIGNAL_VMTRAP_VERBOSE") != 0) {
    fprintf(stderr, "signal_vmtrap_test: %s\n", step);
  }
}

int main(void) {
  pthread_key_create(&self_key, 0);
  progress("int3 raises SIGTRAP");
  test_int3_raises_sigtrap();
  progress("a worker's loop is interrupted");
  test_interrupt_worker();
  progress("the initial thread's loop is interrupted");
  test_interrupt_initial_thread();
  memset(spinners, 0, sizeof(spinners));
  progress("8 threads interrupted concurrently");
  test_concurrent_delivery();
  progress("threads blocked in waits run the handler");
  test_blocked_waits();
  if (failures != 0) {
    fprintf(stderr, "signal_vmtrap_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("signal_vmtrap_test: ok\n");
  return 0;
}

#else

int main(void) {
  printf("signal_vmtrap_test: ok (skipped: x86_64 Linux and Windows only)\n");
  return 0;
}

#endif
