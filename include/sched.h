#ifndef CRT_SCHED_H
#define CRT_SCHED_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sched_param {
  int sched_priority;
};

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
#define SCHED_BATCH 3
#define SCHED_IDLE 5

int sched_yield(void);
/* Bionic <sched.h>. Linux has the real scheduler; on macOS and Windows only
 * SCHED_OTHER exists (priority range 0..0) and any other policy fails with
 * ENOTSUP. */
int sched_get_priority_min(int policy);
int sched_get_priority_max(int policy);
int sched_getscheduler(pid_t pid);
int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param);
int sched_getparam(pid_t pid, struct sched_param* param);
int sched_setparam(pid_t pid, const struct sched_param* param);

#ifdef __cplusplus
}
#endif

#endif
