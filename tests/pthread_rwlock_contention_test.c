#include <pthread.h>
#include <stdio.h>

#define READER_COUNT 4
#define WRITER_ITERATIONS 200

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static int start_readers;
static int shared_value;
static int reader_errors;

static int fail(const char* message) {
  fprintf(stderr, "pthread_rwlock_contention_test: %s\n", message);
  return 1;
}

static void* reader(void* arg) {
  int i;
  (void)arg;

  if (pthread_mutex_lock(&gate_mutex) != 0) {
    return (void*)1;
  }
  while (!start_readers) {
    if (pthread_cond_wait(&gate_cond, &gate_mutex) != 0) {
      pthread_mutex_unlock(&gate_mutex);
      return (void*)2;
    }
  }
  if (pthread_mutex_unlock(&gate_mutex) != 0) {
    return (void*)3;
  }

  for (i = 0; i < WRITER_ITERATIONS; ++i) {
    int first;
    int second;

    if (pthread_rwlock_rdlock(&rwlock) != 0) {
      return (void*)4;
    }
    first = shared_value;
    second = shared_value;
    if (first != second) {
      ++reader_errors;
    }
    if (pthread_rwlock_unlock(&rwlock) != 0) {
      return (void*)5;
    }
  }

  return 0;
}

int main(void) {
  pthread_t readers[READER_COUNT];
  int i;

  for (i = 0; i < READER_COUNT; ++i) {
    if (pthread_create(&readers[i], 0, reader, 0) != 0) {
      return fail("pthread_create reader");
    }
  }

  if (pthread_mutex_lock(&gate_mutex) != 0) {
    return fail("gate lock");
  }
  start_readers = 1;
  if (pthread_cond_broadcast(&gate_cond) != 0 ||
      pthread_mutex_unlock(&gate_mutex) != 0) {
    return fail("gate broadcast");
  }

  for (i = 0; i < WRITER_ITERATIONS; ++i) {
    if (pthread_rwlock_wrlock(&rwlock) != 0) {
      return fail("writer lock");
    }
    ++shared_value;
    if (pthread_rwlock_unlock(&rwlock) != 0) {
      return fail("writer unlock");
    }
  }

  for (i = 0; i < READER_COUNT; ++i) {
    void* result = 0;
    if (pthread_join(readers[i], &result) != 0 || result != 0) {
      return fail("reader join");
    }
  }
  if (reader_errors != 0 || shared_value != WRITER_ITERATIONS) {
    return fail("rwlock protected value");
  }

  printf("pthread_rwlock_contention_test: ok\n");
  return 0;
}
