/* The signal semantics a JIT's VM traps depend on (WTF/JavaScriptCore with JSC_usePollingTraps off),
 * x86_64 Linux, Windows/x64 and Windows/arm64. JavaScriptCore interrupts compiled code that makes no calls and waits
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

#if (defined(__x86_64__) && (defined(__linux__) || defined(CRT_TARGET_OS_WINDOWS))) || \
    (defined(__aarch64__) && defined(CRT_TARGET_OS_WINDOWS))

#if defined(__aarch64__)
/* A handler on Windows/arm64 gets the Linux aarch64 ucontext (the header's own ucontext_t is private there). */
#include <private/crt_linux_ucontext_aarch64.h>
typedef struct crt_linux_aarch64_ucontext handler_context;
#define UC_PC(uc) ((uc)->uc_mcontext.pc)
#define UC_SP(uc) ((uc)->uc_mcontext.sp)
#else
typedef ucontext_t handler_context;
#define UC_PC(uc) ((uc)->uc_mcontext.gregs[REG_RIP])
#define UC_SP(uc) ((uc)->uc_mcontext.gregs[REG_RSP])
#endif

static int failures;

#define CHECK(condition, message)                                                    \
  do {                                                                               \
    if (!(condition)) {                                                              \
      fprintf(stderr, "signal_vmtrap_test: %s (line %d)\n", message, __LINE__);      \
      ++failures;                                                                    \
    }                                                                                \
  } while (0)

#define MAX_THREADS 8 /* busy-looping threads: the runtime is rounds x slices, so the round counts below are small */

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
  volatile int finished;   /* set by the thread just before it returns */
};

static struct spinner spinners[MAX_THREADS];
static pthread_key_t self_key;

/* Spins in a loop that never calls anything and never blocks; leaves it only when the handler
 * moves RIP from the loop to `after`. */
static void spin(struct spinner* me) {
#if defined(__aarch64__)
  __asm__ volatile(
      "adr x9, 1f\n\t"
      "str x9, %0\n\t"
      "adr x9, 2f\n\t"
      "str x9, %1\n\t"
      "mov w9, #1\n\t"
      "str w9, %2\n\t"
      "1: b 1b\n\t"
      "2:\n\t"
      : "=m"(me->loop_address), "=m"(me->after_address), "=m"(me->ready)
      :
      : "x9", "memory");
#else
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
#endif
}

static void on_usr1(int sig, siginfo_t* info, void* context) {
  handler_context* uc = (handler_context*)context;
  struct spinner* me = (struct spinner*)pthread_getspecific(self_key);
  uintptr_t rip = (uintptr_t)UC_PC(uc);
  uintptr_t rsp = (uintptr_t)UC_SP(uc);
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
    UC_PC(uc) = (uintptr_t)me->after_address;
  }
}

static void* spinner_main(void* argument) {
  struct spinner* me = (struct spinner*)argument;

  pthread_setspecific(self_key, me);
  for (;;) {
    if (me->stop) break;
    me->ready = 0;
    spin(me);
    me->escaped = 1;
    while (me->escaped) {
      /* wait until the controller has seen the escape and re-armed us */
      if (me->stop) break;
      sched_yield();
    }
    if (me->stop) break;
  }
  me->finished = 1;
  return 0;
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
  double deadline = now() + 30.0;
  int attempts = 0;

  while (!*escaped) {
    double patience;

    if (now() > deadline) return 0;
    ++attempts;
    if (sent != 0) ++*sent;
    if (pthread_kill(thread, sig) != 0) return 0;
    /* Give the signal time to take before sending another (a second one merges into a pending one). By
     * the clock, not by a count of yields: on one core every yield hands the CPU to a spinning thread for
     * a whole time slice. */
    for (patience = now() + 0.05; !*escaped && now() < patience;) sched_yield();
  }
  return attempts != 0 ? attempts : 1; /* a late signal of the previous round may have escaped it already */
}

/* Teardown: a spinner may already be back in its loop, so interrupt it once more after asking it to stop. */
static void stop_spinner(struct spinner* s) {
  double deadline = now() + 30.0;

  s->stop = 1;
  /* `escaped` may be stale (a signal of an earlier round escaped the loop again): clear it, signal until
   * the thread says it has returned. */
  while (!s->finished && now() < deadline) {
    double patience;

    s->escaped = 0;
    pthread_kill(s->thread, SIGUSR1);
    for (patience = now() + 0.05; !s->finished && now() < patience;) sched_yield();
  }
  CHECK(s->finished, "a spinner returned at teardown");
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
  for (round = 0; round < 40; ++round) {
    if (!wait_for(&s->ready, 1, 30.0)) {
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
   * loop was left through the handler) and never more often than a signal was sent. A handler may run
   * in a later round than its send, but both counters cover the whole test. */
  CHECK(s->handled >= 40 && s->handled <= sent,
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
  handler_context* uc = (handler_context*)context;

  (void)info;
  (void)sig;
  if ((void*)(uintptr_t)UC_PC(uc) == initial_loop) {
    initial_context_ok = 1;
    UC_PC(uc) = (uintptr_t)initial_after;
  }
}

static void* poke_initial(void* argument) {
  int i;

  (void)argument;
  for (i = 0; i < 20; ++i) {
    wait_for(&initial_ready, 1, 30.0);
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
  for (i = 0; i < 20; ++i) {
    initial_context_ok = 0;
#if defined(__aarch64__)
    __asm__ volatile(
        "adr x9, 1f\n\t"
        "str x9, %0\n\t"
        "adr x9, 2f\n\t"
        "str x9, %1\n\t"
        "mov w9, #1\n\t"
        "str w9, %2\n\t"
        "1: b 1b\n\t"
        "2:\n\t"
        : "=m"(initial_loop), "=m"(initial_after), "=m"(initial_ready)
        :
        : "x9", "memory");
#else
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
#endif
    if (!initial_context_ok) ok = 0;
    initial_escaped = 1;
    if (!wait_for(&initial_ready, 0, 30.0)) {
      ok = 0;
      break;
    }
  }
  pthread_join(poker, 0);
  CHECK(ok, "the initial thread's loop is interrupted by another thread, 20 times");
}

#if defined(__aarch64__)
#define TRAP_INSTRUCTION "brk #0xf000"
#else
#define TRAP_INSTRUCTION "int3"
#endif

/* A patched-in `int3` (arm64: `brk #0xf000`) raises SIGTRAP in the executing thread; the handler may move RIP. */
static volatile int trap_seen;
static volatile int trap_redirect_seen;
static void* volatile trap_after;

static void on_trap(int sig, siginfo_t* info, void* context) {
  handler_context* uc = (handler_context*)context;

  (void)info;
  if (sig == SIGTRAP) {
    ++trap_seen;
    if (trap_after != 0) {
      UC_PC(uc) = (uintptr_t)trap_after;
      trap_redirect_seen = 1;
    }
#if defined(__aarch64__)
    else {
      UC_PC(uc) += 4; /* arm64 reports the brk itself (as Linux does): step over it */
    }
#endif
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
  __asm__ volatile(TRAP_INSTRUCTION ::: "memory");
  CHECK(trap_seen == 1, "an int3 ran the SIGTRAP handler once and execution continued after it");

  trap_after = (void*)1; /* set below to the label after the trap */
#if defined(__aarch64__)
  __asm__ volatile(
      "adr x9, 2f\n\t"
      "str x9, %0\n\t"
      TRAP_INSTRUCTION "\n\t"
      "mov w9, #0\n\t"
      "str w9, %1\n\t"
      "2:\n\t"
      : "=m"(trap_after), "=m"(skipped)
      :
      : "x9", "memory");
#else
  __asm__ volatile(
      "lea 2f(%%rip), %%rax\n\t"
      "movq %%rax, %0\n\t"
      "int3\n\t"
      "movl $0, %1\n\t"
      "2:\n\t"
      : "=m"(trap_after), "=m"(skipped)
      :
      : "rax", "memory");
#endif
  CHECK(trap_redirect_seen == 1 && skipped == 1, "the SIGTRAP handler rewrote REG_RIP and the thread resumed there");
  trap_after = 0;
  signal(SIGTRAP, SIG_DFL);
}

/* Many threads, interrupted concurrently from one controller and from each other. */
static volatile long total_handled;
static volatile long total_sent;

static void on_usr1_many(int sig, siginfo_t* info, void* context) {
  handler_context* uc = (handler_context*)context;
  struct spinner* me = (struct spinner*)pthread_getspecific(self_key);

  (void)sig;
  (void)info;
  if (me == 0) return;
  __atomic_fetch_add(&me->handled, 1, __ATOMIC_SEQ_CST);
  __atomic_fetch_add(&total_handled, 1, __ATOMIC_SEQ_CST);
  if ((void*)(uintptr_t)UC_PC(uc) == me->loop_address) {
    UC_PC(uc) = (uintptr_t)me->after_address;
  }
}

static void test_concurrent_delivery(void) {
  struct sigaction action;
  int i, round;
  int ok = 1;
  const int threads = MAX_THREADS;
  const int rounds = 30;

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
      if (!wait_for(&spinners[i].ready, 1, 30.0)) ok = 0;
    }
    for (i = 0; i < threads && ok; ++i) {
      long sent = 0;

      /* All eight are interrupted close together: the sends are interleaved by taking the threads in turn. */
      if (interrupt_until_escaped(spinners[i].thread, SIGUSR1, &spinners[i].escaped, &sent) == 0) ok = 0;
      __atomic_fetch_add(&total_sent, sent, __ATOMIC_SEQ_CST);
    }
    for (i = 0; i < threads; ++i) spinners[i].escaped = 0;
  }
  CHECK(ok, "8 threads interrupted concurrently, 30 rounds, no hang");
  if (total_handled < (long)threads * rounds || total_handled > total_sent) {
    fprintf(stderr, "signal_vmtrap_test: concurrent handled=%ld sent=%ld minimum=%d\n",
            total_handled, total_sent, threads * rounds);
  }
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
      struct timespec request = {2, 0};

      block_in_call = 1;
      /* Long enough that the handler runs while it sleeps; the loop (not one long sleep) ends it, so a
       * release that arrives before the thread is in nanosleep() cannot leave it asleep. */
      while (!block_release) nanosleep(&request, 0);
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

    if (getenv("SIGNAL_VMTRAP_VERBOSE") != 0) fprintf(stderr, "signal_vmtrap_test:   %s\n", names[kind]);
    block_release = 0;
    block_handled = 0;
    block_in_call = 0;
    pthread_create(&thread, 0, blocked_main, (void*)(intptr_t)kind);
    wait_for(&block_in_call, 1, 30.0);
    usleep(50 * 1000); /* now parked in the call */
    pthread_kill(thread, SIGUSR1);
    snprintf(message, sizeof(message), "the handler ran on a thread blocked in %s", names[kind]);
    CHECK(wait_for(&block_handled, 1, 30.0), message);
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

/* `hlt` -- the instruction JavaScriptCore patches into compiled code as its VM trap -- is a general-protection
 * fault: Linux delivers SIGSEGV (si_code SI_KERNEL, no address), Windows reports a privileged-instruction
 * exception, which the CRT must turn into the same SIGSEGV. A signal sent to a thread while it is inside
 * that handler (JavaScriptCore blocks every signal there) must be delivered when the handler returns,
 * not be lost: its thread-suspend request would wait for ever. */
static volatile int hlt_seen;
static volatile int hlt_code;
static volatile void* hlt_addr;
static volatile int hlt_in_handler;
static volatile int hlt_usr1_sent;
static volatile int hlt_usr1_ran;
static pthread_t hlt_target;

static void on_hlt_segv(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;
  double deadline = now() + 10.0;

  (void)sig;
  hlt_code = info->si_code;
  hlt_addr = info->si_addr;
  hlt_seen = 1;
  uc->uc_mcontext.gregs[REG_RIP] += 1; /* step over the one-byte hlt */
  hlt_in_handler = 1;
  while (!hlt_usr1_sent && now() < deadline) sched_yield();
  CHECK(!hlt_usr1_ran, "SIGUSR1 stayed blocked while the SIGSEGV handler ran");
}

static void on_hlt_usr1(int sig) {
  (void)sig;
  hlt_usr1_ran = 1;
}

static void* hlt_sender(void* argument) {
  (void)argument;
  wait_for(&hlt_in_handler, 1, 10.0);
  pthread_kill(hlt_target, SIGUSR1);
  hlt_usr1_sent = 1;
  return 0;
}

static void test_hlt_raises_sigsegv(void) {
  struct sigaction action;
  pthread_t sender;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_hlt_segv;
  action.sa_flags = SA_SIGINFO;
  sigfillset(&action.sa_mask);
  CHECK(sigaction(SIGSEGV, &action, 0) == 0, "sigaction(SIGSEGV)");
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_hlt_usr1;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGUSR1, &action, 0) == 0, "sigaction(SIGUSR1)");

  hlt_target = pthread_self();
  pthread_create(&sender, 0, hlt_sender, 0);
  __asm__ volatile("hlt" ::: "memory");
  pthread_join(sender, 0);
  CHECK(hlt_seen == 1, "hlt ran the SIGSEGV handler and execution continued after it");
  CHECK(hlt_code == 0x80, "the hlt fault carries si_code SI_KERNEL");
  CHECK(hlt_addr == 0, "the hlt fault carries no address");
  CHECK(hlt_usr1_ran == 1, "a SIGUSR1 sent during the SIGSEGV handler was delivered when it returned");
  signal(SIGSEGV, SIG_DFL);
  signal(SIGUSR1, SIG_DFL);
}

#if defined(__x86_64__)
/* A fault taken while another thread is delivering a signal to the faulting thread. On Windows the sender
 * redirects the target (SetThreadContext) at an arbitrary instruction; when that lands after the kernel built
 * a fault's exception record but before it captured the context, the fault handler used to see the signal stub
 * as the interrupted pc (JavaScriptCore's Wasm fault handler then refused the fault and the process died with
 * SIGSEGV). The handler must always see the faulting instruction itself, and a pc it rewrites must take effect. */
static void* volatile fault_site;
static volatile int fault_handled;
static volatile int fault_wrong_pc;
static volatile int fault_usr1_ran;
static volatile int fault_done;

static void on_fault_segv(int sig, siginfo_t* info, void* context) {
  ucontext_t* uc = (ucontext_t*)context;

  (void)sig;
  (void)info;
  if ((void*)UC_PC(uc) != fault_site) {
    ++fault_wrong_pc;
    UC_PC(uc) = (greg_t)(uintptr_t)fault_site;
  }
  UC_PC(uc) += 1; /* step over the one-byte hlt */
  ++fault_handled;
}

static void on_fault_usr1(int sig) {
  (void)sig;
  ++fault_usr1_ran;
}

static pthread_t fault_target;

static void* fault_sender(void* argument) {
  (void)argument;
  while (!fault_done) {
    pthread_kill(fault_target, SIGUSR1);
    sched_yield();
  }
  return 0;
}

static void test_fault_while_signalled(void) {
  struct sigaction action;
  pthread_t sender;
  double deadline = now() + 3.0;
  int faults = 0;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_fault_segv;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGSEGV, &action, 0) == 0, "sigaction(SIGSEGV)");
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_fault_usr1;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGUSR1, &action, 0) == 0, "sigaction(SIGUSR1)");

  fault_target = pthread_self();
  pthread_create(&sender, 0, fault_sender, 0);
  while (now() < deadline && faults < 200000) {
    void* site;
    __asm__ volatile("leaq 1f(%%rip), %0\n\t"
                     "movq %0, %1\n\t"
                     "1: hlt"
                     : "=&r"(site), "=m"(fault_site)::"memory");
    ++faults;
  }
  fault_done = 1;
  pthread_join(sender, 0);
  CHECK(fault_handled == faults, "every fault ran the handler exactly once and execution continued");
  CHECK(fault_wrong_pc == 0, "the fault handler always saw the faulting instruction as the interrupted pc");
  CHECK(fault_usr1_ran > 0, "signals were delivered to the faulting thread meanwhile");
  signal(SIGSEGV, SIG_DFL);
  signal(SIGUSR1, SIG_DFL);
}
#endif

static void progress(const char* step) {
  if (getenv("SIGNAL_VMTRAP_VERBOSE") != 0) {
    fprintf(stderr, "signal_vmtrap_test: %s\n", step);
  }
}

int main(void) {
  pthread_key_create(&self_key, 0);
  progress("int3 raises SIGTRAP");
  test_int3_raises_sigtrap();
  progress("hlt raises SIGSEGV, pending signals follow the fault handler");
  test_hlt_raises_sigsegv();
  progress("a worker's loop is interrupted");
  test_interrupt_worker();
  progress("the initial thread's loop is interrupted");
  test_interrupt_initial_thread();
  memset(spinners, 0, sizeof(spinners));
  progress("8 threads interrupted concurrently");
  test_concurrent_delivery();
  progress("threads blocked in waits run the handler");
  test_blocked_waits();
#if defined(__x86_64__)
  progress("faults taken while signals are being delivered");
  test_fault_while_signalled();
#endif
  if (failures != 0) {
    fprintf(stderr, "signal_vmtrap_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("signal_vmtrap_test: ok\n");
  return 0;
}

#else

int main(void) {
  printf("signal_vmtrap_test: ok (skipped: x86_64 Linux, Windows x64 and arm64 only)\n");
  return 0;
}

#endif
