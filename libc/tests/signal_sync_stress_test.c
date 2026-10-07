/* pthread mutexes and condition variables while signals are delivered to the threads using them. WTF's
 * Lock/ParkingLot (everything in JavaScriptCore) is built on them, and JavaScriptCore sends its worker
 * threads signals constantly (thread suspend, VM traps). A handler that runs inside a wait, or between a
 * lock and its unlock, must leave mutual exclusion and wake-ups intact:
 *   - at most one thread is ever inside the critical section;
 *   - a counter updated under the lock ends at exactly the expected value;
 *   - threads parked in pthread_cond_wait/_timedwait are woken and finish (no lost wake-up). */
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

#if defined(__linux__) || (defined(__APPLE__) && defined(__aarch64__)) || defined(CRT_TARGET_OS_WINDOWS)

#define WORKERS 6
#define ROUNDS 20000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static volatile int inside;
static volatile int violations;
static volatile long counter;
static volatile long handled;
static volatile int workers_done;
static pthread_t threads[WORKERS];

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static void on_usr1(int sig) {
  (void)sig;
  __atomic_fetch_add(&handled, 1, __ATOMIC_SEQ_CST);
}

static void enter(void) {
  if (__atomic_add_fetch(&inside, 1, __ATOMIC_SEQ_CST) != 1) {
    __atomic_add_fetch(&violations, 1, __ATOMIC_SEQ_CST);
  }
}

static void leave(void) {
  __atomic_sub_fetch(&inside, 1, __ATOMIC_SEQ_CST);
}

static void* worker(void* argument) {
  long id = (long)(intptr_t)argument;
  int i;

  for (i = 0; i < ROUNDS; ++i) {
    pthread_mutex_lock(&lock);
    enter();
    counter = counter + 1;
    if ((i + id) % 64 == 0) {
      /* A bounded wait inside the critical section: the mutex is released while it waits. */
      struct timespec until;

      clock_gettime(CLOCK_REALTIME, &until);
      until.tv_nsec += 200000;
      if (until.tv_nsec >= 1000000000L) {
        until.tv_sec += 1;
        until.tv_nsec -= 1000000000L;
      }
      leave();
      pthread_cond_timedwait(&cond, &lock, &until);
      enter();
    }
    if (i % 256 == 0) {
      pthread_cond_broadcast(&cond);
    }
    leave();
    pthread_mutex_unlock(&lock);
  }
  __atomic_add_fetch(&workers_done, 1, __ATOMIC_SEQ_CST);
  return 0;
}

static void* signaler(void* argument) {
  int i;

  (void)argument;
  while (workers_done < WORKERS) {
    for (i = 0; i < WORKERS; ++i) {
      pthread_kill(threads[i], SIGUSR1);
    }
    sched_yield();
  }
  return 0;
}

/* Parked waiters: they must all be woken by broadcasts that arrive among signals. */
static volatile int parked;
static volatile int released;

static void* parked_waiter(void* argument) {
  (void)argument;
  pthread_mutex_lock(&lock);
  parked = parked + 1;
  while (!released) {
    pthread_cond_wait(&cond, &lock);
  }
  pthread_mutex_unlock(&lock);
  return 0;
}

#if defined(__x86_64__)
/* The flags register must survive an interrupting signal: WTF's Lock is `lock cmpxchg` followed by a branch on
 * ZF, and a signal landing between the two must hand ZF (and CF) back unchanged, or two threads would both
 * believe they took the lock. */
static volatile int flags_stop;
static volatile long flags_errors;
static volatile long flags_iterations;

static void* flags_spinner(void* argument) {
  (void)argument;
  while (!flags_stop) {
    unsigned int cell = 0;
    long bad = 0;

    __asm__ volatile(
        "xorl %%eax, %%eax\n\t"
        "movl $1, %%ecx\n\t"
        "lock cmpxchgl %%ecx, %0\n\t"
        "jne 1f\n\t"
        "stc\n\t"
        "jnc 1f\n\t"
        "clc\n\t"
        "jc 1f\n\t"
        "xorl %%eax, %%eax\n\t"
        "movl $2, %%ecx\n\t"
        "lock cmpxchgl %%ecx, %0\n\t"
        "je 1f\n\t"
        "jmp 2f\n\t"
        "1: incq %1\n\t"
        "2:"
        : "+m"(cell), "+m"(bad)
        :
        : "eax", "ecx", "cc", "memory");
    flags_errors += bad;
    ++flags_iterations;
  }
  return 0;
}

static int test_flags_survive(void) {
  pthread_t spinner;
  double until = now() + 2.0;

  pthread_create(&spinner, 0, flags_spinner, 0);
  while (now() < until) {
    pthread_kill(spinner, SIGUSR1);
    sched_yield();
  }
  flags_stop = 1;
  pthread_join(spinner, 0);
  if (flags_errors != 0) {
    fprintf(stderr, "signal_sync_stress_test: %ld branch(es) went the wrong way after a signal (%ld iterations)\n",
            flags_errors, flags_iterations);
    return 1;
  }
  return 0;
}
#endif

int main(void) {
  struct sigaction action;
  pthread_t sender;
  pthread_t waiters[4];
  double deadline;
  int failures = 0;
  long i;

  memset(&action, 0, sizeof(action));
  action.sa_handler = on_usr1;
  sigemptyset(&action.sa_mask);
  sigaction(SIGUSR1, &action, 0);

#if defined(__x86_64__)
  failures += test_flags_survive();
#endif
  for (i = 0; i < WORKERS; ++i) pthread_create(&threads[i], 0, worker, (void*)(intptr_t)i);
  pthread_create(&sender, 0, signaler, 0);
  for (i = 0; i < WORKERS; ++i) pthread_join(threads[i], 0);
  pthread_join(sender, 0);
  if (violations != 0) {
    fprintf(stderr, "signal_sync_stress_test: %d mutual-exclusion violation(s)\n", violations);
    ++failures;
  }
  if (counter != (long)WORKERS * ROUNDS) {
    fprintf(stderr, "signal_sync_stress_test: counter %ld, expected %ld\n", counter, (long)WORKERS * ROUNDS);
    ++failures;
  }
  if (handled == 0) {
    fprintf(stderr, "signal_sync_stress_test: no signal was delivered\n");
    ++failures;
  }

  /* Lost wake-ups: park waiters, then wake them while signals keep arriving. */
  workers_done = 0;
  for (i = 0; i < 4; ++i) pthread_create(&waiters[i], 0, parked_waiter, 0);
  for (i = 0; i < 4; ++i) pthread_create(&threads[i], 0, worker, (void*)(intptr_t)i); /* reuse as signal targets */
  for (i = 0; i < 4; ++i) pthread_join(threads[i], 0);
  deadline = now() + 20.0;
  while (parked < 4 && now() < deadline) sched_yield();
  pthread_mutex_lock(&lock);
  released = 1;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&lock);
  for (i = 0; i < 4; ++i) {
    pthread_kill(waiters[i], SIGUSR1);
    pthread_join(waiters[i], 0);
  }
  if (failures != 0) {
    fprintf(stderr, "signal_sync_stress_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("signal_sync_stress_test: ok\n");
  return 0;
}

#else

int main(void) {
  printf("signal_sync_stress_test: ok (skipped)\n");
  return 0;
}

#endif
