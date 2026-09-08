#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_bionic_api_test: %s\n", message);
  return 1;
}

static void* worker(void* arg) {
  /* Deliberately does not call pthread_setname_np(pthread_self(), ...)
   * here: the parent's own pthread_setname_np(thread, "child") call
   * right after pthread_create() (below) writes the exact same
   * crt_pthread_control::context.name this thread's own self-rename
   * would -- two unsynchronized, conflicting writes to the same field
   * from two threads is a genuine data race, not a false alarm: caught
   * for real ("pthread_bionic_api_test: name child" failing) when
   * scheduling happened to interleave the two writes so the parent's
   * later pthread_getname_np(thread, ...) observed "worker" (or a
   * partially-overwritten name) instead of the "child" it had just set.
   * Self-rename (pthread_setname_np() on one's own pthread_self()) is
   * already covered separately, race-free, by main()'s own check on
   * itself above -- this thread has nothing left to prove by racing to
   * rename itself too. */
  (void)arg;
  return 0;
}

int main(void) {
  pthread_attr_t attr;
  pthread_t thread;
  void* result = 0;
  char name[16];
  clockid_t clock_id;
  pid_t tid;
  size_t guard_size = 0;

  if (pthread_getattr_np(pthread_self(), &attr) != 0 ||
      pthread_attr_getguardsize(&attr, &guard_size) != 0 ||
      guard_size < 4096) {
    return fail("getattr self");
  }
  if (pthread_setname_np(pthread_self(), "main") != 0 ||
      pthread_getname_np(pthread_self(), name, sizeof(name)) != 0 ||
      strcmp(name, "main") != 0 ||
      pthread_setname_np(pthread_self(), "name-is-too-long") != ERANGE ||
      pthread_getname_np(pthread_self(), name, 2) != ERANGE) {
    return fail("name self");
  }
  tid = pthread_gettid_np(pthread_self());
  if (tid <= 0) {
    return fail("gettid self");
  }
  if (pthread_getcpuclockid(pthread_self(), &clock_id) != ENOTSUP ||
      pthread_getcpuclockid(0, &clock_id) != EINVAL) {
    return fail("cpuclock policy");
  }

  if (pthread_create(&thread, 0, worker, 0) != 0) {
    return fail("pthread_create");
  }
  if (pthread_setname_np(thread, "child") != 0 ||
      pthread_getname_np(thread, name, sizeof(name)) != 0 ||
      strcmp(name, "child") != 0) {
    return fail("name child");
  }
  if (pthread_getattr_np(thread, &attr) != 0 ||
      pthread_attr_getguardsize(&attr, &guard_size) != 0 ||
      guard_size < 4096) {
    return fail("getattr child");
  }
  tid = pthread_gettid_np(thread);
  if (tid <= 0) {
    return fail("gettid child");
  }
  if (pthread_join(thread, &result) != 0 || result != 0) {
    return fail("pthread_join");
  }

  printf("pthread_bionic_api_test: ok\n");
  return 0;
}
