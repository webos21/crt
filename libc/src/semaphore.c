/* POSIX unnamed semaphores, built over the same private futex/wait-address
 * primitive (__crt_wait32/__crt_wake32_*) that already backs
 * pthread_mutex/pthread_cond/pthread_rwlock (see libc/src/pthread.c) --
 * this was the most surprising gap found in the 2026-08-16 Bionic libc
 * audit (docs/bionic_libc_gaps.md) given how complete the rest of the
 * pthread story already is: every primitive a semaphore needs was already
 * sitting right there. Named semaphores (sem_open/sem_close/sem_unlink)
 * match real Bionic's own policy of declaring but never supporting them
 * (ENOSYS) -- Android has never implemented POSIX named semaphores either.
 */
#include <semaphore.h>

#include <errno.h>
#include <time.h>

#include <private/crt_atomic.h>
#include <private/crt_wait.h>

static crt_atomic_int* sem_count(sem_t* sem) {
  return (crt_atomic_int*)&sem->__private[0];
}

/* Small local equivalent of pthread.c's own realtime_until() static
 * helper -- deliberately not shared/exported across files; both are tiny,
 * self-contained, and this keeps semaphore.c independent of pthread.c's
 * internals. */
static int semaphore_time_remaining(const struct timespec* abstime, struct timespec* remaining) {
  struct timespec now;
  time_t sec;
  long nsec;

  if (abstime == 0 || remaining == 0 || abstime->tv_nsec < 0 ||
      abstime->tv_nsec >= 1000000000L) {
    return EINVAL;
  }
  if (abstime->tv_sec < 0 || (abstime->tv_sec == 0 && abstime->tv_nsec == 0)) {
    return ETIMEDOUT;
  }
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    return errno;
  }

  sec = abstime->tv_sec - now.tv_sec;
  nsec = abstime->tv_nsec - now.tv_nsec;
  if (nsec < 0) {
    --sec;
    nsec += 1000000000L;
  }
  if (sec < 0 || (sec == 0 && nsec <= 0)) {
    return ETIMEDOUT;
  }
  remaining->tv_sec = sec;
  remaining->tv_nsec = nsec;
  return 0;
}

int sem_init(sem_t* sem, int pshared, unsigned int value) {
  if (sem == 0) {
    errno = EINVAL;
    return -1;
  }
  if (value > (unsigned int)SEM_VALUE_MAX) {
    errno = EINVAL;
    return -1;
  }
  if (pshared != 0) {
    /* Matches PTHREAD_PROCESS_SHARED across pthread_mutex/rwlock/spinlock
     * in this project -- cross-process shared-memory synchronization
     * policy isn't defined yet. */
    errno = ENOTSUP;
    return -1;
  }
  sem->__private[0] = (int)value;
  return 0;
}

int sem_destroy(sem_t* sem) {
  if (sem == 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

int sem_post(sem_t* sem) {
  crt_atomic_int* count;
  int value;

  if (sem == 0) {
    errno = EINVAL;
    return -1;
  }
  count = sem_count(sem);
  value = crt_atomic_load_acquire(count);
  for (;;) {
    if (value >= SEM_VALUE_MAX) {
      errno = EOVERFLOW;
      return -1;
    }
    if (crt_atomic_compare_exchange_acq_rel(count, &value, value + 1)) {
      break;
    }
  }
  __crt_wake32_one(&sem->__private[0]);
  return 0;
}

int sem_trywait(sem_t* sem) {
  crt_atomic_int* count;
  int value;

  if (sem == 0) {
    errno = EINVAL;
    return -1;
  }
  count = sem_count(sem);
  value = crt_atomic_load_acquire(count);
  while (value > 0) {
    if (crt_atomic_compare_exchange_acq_rel(count, &value, value - 1)) {
      return 0;
    }
  }
  errno = EAGAIN;
  return -1;
}

int sem_wait(sem_t* sem) {
  crt_atomic_int* count;
  int value;

  if (sem == 0) {
    errno = EINVAL;
    return -1;
  }
  count = sem_count(sem);
  for (;;) {
    value = crt_atomic_load_acquire(count);
    while (value > 0) {
      if (crt_atomic_compare_exchange_acq_rel(count, &value, value - 1)) {
        return 0;
      }
    }
    {
      int result = __crt_wait32(&sem->__private[0], 0);

      if (result != 0 && result != EINTR && result != EAGAIN) {
        errno = result;
        return -1;
      }
    }
  }
}

int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
  crt_atomic_int* count;
  int value;

  if (sem == 0 || abs_timeout == 0) {
    errno = EINVAL;
    return -1;
  }
  if (abs_timeout->tv_nsec < 0 || abs_timeout->tv_nsec >= 1000000000L) {
    errno = EINVAL;
    return -1;
  }
  count = sem_count(sem);
  for (;;) {
    struct timespec remaining;
    int result;

    value = crt_atomic_load_acquire(count);
    while (value > 0) {
      if (crt_atomic_compare_exchange_acq_rel(count, &value, value - 1)) {
        return 0;
      }
    }

    result = semaphore_time_remaining(abs_timeout, &remaining);
    if (result != 0) {
      errno = result;
      return -1;
    }
    result = __crt_wait32_timed(&sem->__private[0], 0, &remaining);
    if (result != 0 && result != EINTR && result != EAGAIN && result != ETIMEDOUT) {
      errno = result;
      return -1;
    }
    if (result == ETIMEDOUT) {
      /* Recheck the count before actually reporting a timeout -- a post()
       * could have raced in right as the timed wait was expiring. */
      value = crt_atomic_load_acquire(count);
      if (value <= 0) {
        errno = ETIMEDOUT;
        return -1;
      }
    }
  }
}

int sem_getvalue(sem_t* sem, int* sval) {
  if (sem == 0 || sval == 0) {
    errno = EINVAL;
    return -1;
  }
  *sval = crt_atomic_load_acquire(sem_count(sem));
  return 0;
}

sem_t* sem_open(const char* name, int oflag, ...) {
  (void)name;
  (void)oflag;
  errno = ENOSYS;
  return SEM_FAILED;
}

int sem_close(sem_t* sem) {
  (void)sem;
  errno = ENOSYS;
  return -1;
}

int sem_unlink(const char* name) {
  (void)name;
  errno = ENOSYS;
  return -1;
}
