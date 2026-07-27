#ifndef CRT_TIME_H
#define CRT_TIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t time_t;
typedef int clockid_t;

struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

time_t time(time_t* tloc);
int clock_gettime(clockid_t clock_id, struct timespec* tp);
int nanosleep(const struct timespec* req, struct timespec* rem);

#ifdef __cplusplus
}
#endif

#endif
