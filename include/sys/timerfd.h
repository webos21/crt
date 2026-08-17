#ifndef CRT_SYS_TIMERFD_H
#define CRT_SYS_TIMERFD_H

/* Linux-only in real Bionic too -- see sys/eventfd.h's own comment for why
 * this is still declared on every host (portable-compile, real-behavior-
 * only-on-Linux), and docs/bionic_libc_gaps.md/HISTORY.md's 2026-08-17
 * entry. Reuses this project's existing struct itimerspec/CLOCK_REALTIME/
 * CLOCK_MONOTONIC from <time.h> rather than redeclaring them. */

#include <fcntl.h> /* O_CLOEXEC/O_NONBLOCK, reused below. */
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFD_CLOEXEC O_CLOEXEC
#define TFD_NONBLOCK O_NONBLOCK
#define TFD_TIMER_ABSTIME 0x00000001
#define TFD_TIMER_CANCEL_ON_SET 0x00000002

int timerfd_create(int clockid, int flags);
int timerfd_settime(
    int fd, int flags, const struct itimerspec* new_value, struct itimerspec* old_value);
int timerfd_gettime(int fd, struct itimerspec* curr_value);

#ifdef __cplusplus
}
#endif

#endif
