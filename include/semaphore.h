#ifndef CRT_SEMAPHORE_H
#define CRT_SEMAPHORE_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bionic-shaped opaque storage, matching the pthread_mutex_t/pthread_cond_t
 * convention already used throughout this project (a small __private[]
 * array manipulated internally rather than a directly-typed field) -- see
 * docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-16 entry. Real Bionic's
 * own sem_t is just a single atomic unsigned int; this keeps the same
 * one-word shape while following this project's established storage
 * convention. */
typedef struct {
  int __private[1];
} sem_t;

#define SEM_FAILED ((sem_t*)0)
#define SEM_VALUE_MAX 0x7fffffff

int sem_init(sem_t* sem, int pshared, unsigned int value);
int sem_destroy(sem_t* sem);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout);
int sem_post(sem_t* sem);
int sem_getvalue(sem_t* sem, int* sval);

/* Named semaphores. Real Bionic declares these too but fails them with
 * ENOSYS at runtime (Android has never supported POSIX named semaphores);
 * matched here rather than omitted, for the same Bionic-parity reason. */
sem_t* sem_open(const char* name, int oflag, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char* name);

#ifdef __cplusplus
}
#endif

#endif
