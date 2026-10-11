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

/* The BSD timeval macros Bionic's <sys/time.h> provides. */
#define timerclear(a) ((a)->tv_sec = (a)->tv_usec = 0)
#define timerisset(a) ((a)->tv_sec || (a)->tv_usec)
#define timercmp(a, b, op) \
  (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec op (b)->tv_usec) : ((a)->tv_sec op (b)->tv_sec))
#define timeradd(a, b, res)                                \
  do {                                                     \
    (res)->tv_sec = (a)->tv_sec + (b)->tv_sec;             \
    (res)->tv_usec = (a)->tv_usec + (b)->tv_usec;          \
    if ((res)->tv_usec >= 1000000) {                       \
      (res)->tv_sec++;                                     \
      (res)->tv_usec -= 1000000;                           \
    }                                                      \
  } while (0)
#define timersub(a, b, res)                                \
  do {                                                     \
    (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;             \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec;          \
    if ((res)->tv_usec < 0) {                              \
      (res)->tv_sec--;                                     \
      (res)->tv_usec += 1000000;                           \
    }                                                      \
  } while (0)

int gettimeofday(struct timeval* tv, void* tz);
int settimeofday(const struct timeval* tv, const void* tz);
int utimes(const char* path, const struct timeval times[2]);
int futimes(int fd, const struct timeval times[2]);

#ifdef __cplusplus
}
#endif

#endif
