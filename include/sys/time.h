#ifndef CRT_SYS_TIME_H
#define CRT_SYS_TIME_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timeval {
  time_t tv_sec;
  long tv_usec;
};

int gettimeofday(struct timeval* tv, void* tz);

#ifdef __cplusplus
}
#endif

#endif
