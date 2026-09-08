#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "pthread_policy_test: %s\n", message);
  return 1;
}

int main(void) {
  pthread_attr_t attr;
  pthread_mutexattr_t mutex_attr;
  pthread_t self = pthread_self();
  struct sched_param param;
  int inheritsched = -1;
  int policy = -1;
  int robust = -1;
  int scope = -1;
  int old_cancel = -1;

  if (pthread_attr_init(&attr) != 0) {
    return fail("attr init");
  }
  if (pthread_attr_getinheritsched(&attr, &inheritsched) != 0 ||
      inheritsched != PTHREAD_INHERIT_SCHED ||
      PTHREAD_EXPLICIT_SCHED != 0 ||
      PTHREAD_INHERIT_SCHED != 1 ||
      pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0 ||
      pthread_attr_getinheritsched(&attr, &inheritsched) != 0 ||
      inheritsched != PTHREAD_EXPLICIT_SCHED ||
      pthread_attr_setinheritsched(&attr, 99) != EINVAL) {
    return fail("inheritsched");
  }
  if (pthread_attr_getschedpolicy(&attr, &policy) != 0 ||
      policy != SCHED_OTHER ||
      pthread_attr_setschedpolicy(&attr, SCHED_OTHER) != 0 ||
      pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0 ||
      pthread_attr_getschedpolicy(&attr, &policy) != 0 ||
      policy != SCHED_FIFO ||
      pthread_attr_setschedpolicy(&attr, 99) != EINVAL) {
    return fail("sched policy");
  }
  param.sched_priority = 0;
  if (pthread_attr_setschedparam(&attr, &param) != 0 ||
      pthread_attr_getschedparam(&attr, &param) != 0 ||
      param.sched_priority != 0) {
    return fail("sched param");
  }
  param.sched_priority = 1;
  if (pthread_attr_setschedparam(&attr, &param) != 0 ||
      pthread_attr_getschedparam(&attr, &param) != 0 ||
      param.sched_priority != 1) {
    return fail("sched param stored");
  }
  if (pthread_attr_getscope(&attr, &scope) != 0 ||
      scope != PTHREAD_SCOPE_SYSTEM ||
      pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0 ||
      pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS) != ENOTSUP ||
      pthread_attr_setscope(&attr, 99) != EINVAL) {
    return fail("scope");
  }
  pthread_attr_destroy(&attr);

  if (pthread_getschedparam(self, &policy, &param) != 0 ||
      policy != SCHED_OTHER ||
      param.sched_priority != 0 ||
      pthread_setschedparam(self, SCHED_OTHER, &param) != 0 ||
      pthread_setschedprio(self, 0) != 0) {
    return fail("thread sched");
  }
  param.sched_priority = 1;
  if (pthread_setschedparam(self, SCHED_OTHER, &param) != ENOTSUP ||
      pthread_setschedprio(self, 1) != ENOTSUP ||
      pthread_getschedparam(0, &policy, &param) != EINVAL ||
      pthread_setschedparam(0, SCHED_OTHER, &param) != EINVAL) {
    return fail("thread sched invalid");
  }

  if (pthread_mutexattr_init(&mutex_attr) != 0 ||
      pthread_mutexattr_getrobust(&mutex_attr, &robust) != 0 ||
      robust != PTHREAD_MUTEX_STALLED ||
      pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_STALLED) != 0 ||
      pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST) != ENOTSUP ||
      pthread_mutexattr_setrobust(&mutex_attr, 99) != EINVAL) {
    return fail("robust policy");
  }
  pthread_mutexattr_destroy(&mutex_attr);

  if (pthread_cancel(0) != EINVAL ||
      pthread_cancel(self) != ENOTSUP ||
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_cancel) != 0 ||
      old_cancel != PTHREAD_CANCEL_DISABLE ||
      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0) != ENOTSUP ||
      pthread_setcancelstate(99, 0) != EINVAL ||
      pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_cancel) != 0 ||
      old_cancel != PTHREAD_CANCEL_DEFERRED ||
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0) != ENOTSUP ||
      pthread_setcanceltype(99, 0) != EINVAL) {
    return fail("cancel policy");
  }
  pthread_testcancel();

  printf("pthread_policy_test: ok\n");
  return 0;
}
