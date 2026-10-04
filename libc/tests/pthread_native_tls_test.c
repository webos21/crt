/* Native thread-local storage in pthread_create()'d threads. On Linux a CRT thread
 * used to keep its creator's thread pointer, so every __thread/thread_local variable
 * was shared by all threads (one address, values bleeding between threads); the thread
 * now gets its own TLS block cloned from the modules' PT_TLS images (see
 * libc/src/arch/linux/common/thread_tls.c). This checks the contract on every host:
 * distinct addresses, pristine initial values (.tdata and .tbss), isolation, and that
 * the creating thread's own copy is untouched. */
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static __thread int counter = 100;       /* .tdata */
static __thread char scratch[64];        /* .tbss */
static __thread uintptr_t identity;      /* .tbss, set to the thread's own marker */

#define THREADS 6
#define ROUNDS 2000

/* Every worker records its TLS address and then waits until all of them have: a thread that
 * exited before the next one started could legitimately have its TLS block reused at the same
 * address (seen as a false "share a TLS address" on a loaded macOS host), so the addresses are
 * only comparable while all the threads are alive. */
static int threads_started;

struct result {
  int initial_counter;
  int initial_scratch_zero;
  int isolated;
  uintptr_t address;
};

static void* worker(void* argument) {
  struct result* result = argument;
  uintptr_t marker = (uintptr_t)argument;
  int i;

  result->initial_counter = counter;
  result->initial_scratch_zero = scratch[0] == 0 && scratch[63] == 0 && identity == 0;
  result->address = (uintptr_t)&counter;
  __atomic_add_fetch(&threads_started, 1, __ATOMIC_SEQ_CST);
  while (__atomic_load_n(&threads_started, __ATOMIC_SEQ_CST) < THREADS) {
    sched_yield();
  }
  identity = marker;
  scratch[0] = (char)(marker & 0x7f);
  result->isolated = 1;
  for (i = 0; i < ROUNDS; ++i) {
    counter += 1;
    if (identity != marker || scratch[0] != (char)(marker & 0x7f) || counter != 101 + i) {
      result->isolated = 0;
      break;
    }
    if ((i & 127) == 0) {
      sched_yield();
    }
  }
  return 0;
}

int main(void) {
  pthread_t threads[THREADS];
  struct result results[THREADS];
  int t;
  int failures = 0;

  counter = 5;
  scratch[0] = 'M';
  identity = 0xabcd;

  for (t = 0; t < THREADS; ++t) {
    memset(&results[t], 0, sizeof(results[t]));
    if (pthread_create(&threads[t], 0, worker, &results[t]) != 0) {
      fprintf(stderr, "pthread_native_tls_test: pthread_create failed\n");
      return 1;
    }
  }
  for (t = 0; t < THREADS; ++t) {
    pthread_join(threads[t], 0);
  }
  for (t = 0; t < THREADS; ++t) {
    int u;

    if (results[t].initial_counter != 100) {
      fprintf(stderr, "pthread_native_tls_test: thread %d saw counter=%d, not the .tdata image\n", t,
              results[t].initial_counter);
      ++failures;
    }
    if (!results[t].initial_scratch_zero) {
      fprintf(stderr, "pthread_native_tls_test: thread %d did not start with zeroed .tbss\n", t);
      ++failures;
    }
    if (!results[t].isolated) {
      fprintf(stderr, "pthread_native_tls_test: thread %d saw another thread's writes\n", t);
      ++failures;
    }
    if (results[t].address == (uintptr_t)&counter) {
      fprintf(stderr, "pthread_native_tls_test: thread %d shares the creator's TLS address\n", t);
      ++failures;
    }
    for (u = t + 1; u < THREADS; ++u) {
      if (results[t].address == results[u].address) {
        fprintf(stderr, "pthread_native_tls_test: threads %d and %d share a TLS address\n", t, u);
        ++failures;
      }
    }
  }
  if (counter != 5 || scratch[0] != 'M' || identity != 0xabcd) {
    fprintf(stderr, "pthread_native_tls_test: the creating thread's TLS was modified (counter=%d)\n", counter);
    ++failures;
  }
  if (failures != 0) {
    return 1;
  }
  printf("pthread_native_tls_test: ok threads=%d rounds=%d\n", THREADS, ROUNDS);
  return 0;
}
