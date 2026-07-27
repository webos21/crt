#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int ready;
static int go_value;
static int observed_value;

static int fail(const char* message) {
  fprintf(stderr, "pthread_cond_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  (void)arg;

  if (pthread_mutex_lock(&mutex) != 0) {
    return (void*)1;
  }
  ready = 1;
  pthread_cond_signal(&cond);

  while (go_value == 0) {
    if (pthread_cond_wait(&cond, &mutex) != 0) {
      pthread_mutex_unlock(&mutex);
      return (void*)2;
    }
  }
  observed_value = go_value;
  if (pthread_mutex_unlock(&mutex) != 0) {
    return (void*)3;
  }
  return &observed_value;
}

int main(void) {
  pthread_cond_t local_cond;
  pthread_condattr_t cond_attr;
  pthread_t thread;
  void* result = 0;

  if (pthread_cond_init(0, 0) != EINVAL ||
      pthread_cond_destroy(0) != EINVAL ||
      pthread_cond_signal(0) != EINVAL ||
      pthread_cond_broadcast(0) != EINVAL ||
      pthread_cond_wait(0, &mutex) != EINVAL ||
      pthread_cond_wait(&cond, 0) != EINVAL) {
    return fail("invalid cond args");
  }
  if (pthread_condattr_init(0) != EINVAL ||
      pthread_condattr_init(&cond_attr) != 0 ||
      pthread_condattr_destroy(&cond_attr) != 0 ||
      pthread_condattr_destroy(0) != EINVAL) {
    return fail("cond attr");
  }
  if (pthread_cond_init(&local_cond, &cond_attr) != 0 ||
      pthread_cond_destroy(&local_cond) != 0) {
    return fail("local cond lifecycle");
  }

  if (pthread_mutex_lock(&mutex) != 0) {
    return fail("main lock");
  }
  if (pthread_create(&thread, 0, worker, 0) != 0) {
    pthread_mutex_unlock(&mutex);
    return fail("pthread_create");
  }
  while (!ready) {
    if (pthread_cond_wait(&cond, &mutex) != 0) {
      pthread_mutex_unlock(&mutex);
      return fail("wait ready");
    }
  }
  go_value = 123;
  if (pthread_cond_broadcast(&cond) != 0) {
    pthread_mutex_unlock(&mutex);
    return fail("broadcast");
  }
  if (pthread_mutex_unlock(&mutex) != 0) {
    return fail("main unlock");
  }
  if (pthread_join(thread, &result) != 0) {
    return fail("pthread_join");
  }
  if (result != &observed_value || observed_value != 123) {
    return fail("observed value");
  }
  if (pthread_cond_destroy(&cond) != 0) {
    return fail("static cond destroy");
  }

  printf("pthread_cond_test: ok\n");
  return 0;
}
