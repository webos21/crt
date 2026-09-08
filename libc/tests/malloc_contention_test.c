#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 4
#define ITERATIONS 200

static int failures;

static int fail(const char* message) {
  fprintf(stderr, "malloc_contention_test: %s\n", message);
  return 1;
}

static void record_failure(void) {
  __atomic_fetch_add(&failures, 1, __ATOMIC_RELAXED);
}

static void* worker(void* arg) {
  size_t base = (size_t)(uintptr_t)arg;
  unsigned int i;

  for (i = 0; i < ITERATIONS; ++i) {
    size_t size = 16 + ((base + i) % 97);
    unsigned char* bytes = (unsigned char*)malloc(size);
    unsigned char* grown;
    size_t j;

    if (bytes == 0) {
      record_failure();
      return 0;
    }
    memset(bytes, (int)(base + i), size);
    for (j = 0; j < size; ++j) {
      if (bytes[j] != (unsigned char)(base + i)) {
        record_failure();
        free(bytes);
        return 0;
      }
    }

    grown = (unsigned char*)realloc(bytes, size + 32);
    if (grown == 0) {
      record_failure();
      free(bytes);
      return 0;
    }
    for (j = 0; j < size; ++j) {
      if (grown[j] != (unsigned char)(base + i)) {
        record_failure();
        free(grown);
        return 0;
      }
    }
    free(grown);
  }

  return arg;
}

int main(void) {
  pthread_t threads[THREAD_COUNT];
  unsigned int i;

  for (i = 0; i < THREAD_COUNT; ++i) {
    if (pthread_create(&threads[i], 0, worker, (void*)(uintptr_t)(i + 1)) != 0) {
      return fail("pthread_create");
    }
  }
  for (i = 0; i < THREAD_COUNT; ++i) {
    void* result = 0;
    if (pthread_join(threads[i], &result) != 0 || result != (void*)(uintptr_t)(i + 1)) {
      return fail("pthread_join");
    }
  }
  if (__atomic_load_n(&failures, __ATOMIC_RELAXED) != 0) {
    return fail("worker failure");
  }

  printf("malloc_contention_test: ok\n");
  return 0;
}
