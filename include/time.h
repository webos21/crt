#ifndef CRT_TIME_H
#define CRT_TIME_H

#include <stddef.h>
#include <bits/crt_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __crt_time_t time_t;
typedef __crt_clock_t clock_t;
typedef __crt_clockid_t clockid_t;

struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCKS_PER_SEC 1000000L
#define TIME_UTC 1
#define UTIME_NOW 1073741823L
#define UTIME_OMIT 1073741822L

clock_t clock(void);
time_t time(time_t* tloc);
int clock_gettime(clockid_t clock_id, struct timespec* tp);
int nanosleep(const struct timespec* req, struct timespec* rem);
int timespec_get(struct timespec* ts, int base);
struct tm* gmtime(const time_t* timep);
struct tm* gmtime_r(const time_t* timep, struct tm* result);
struct tm* localtime(const time_t* timep);
struct tm* localtime_r(const time_t* timep, struct tm* result);
char* asctime(const struct tm* tm);
char* asctime_r(const struct tm* tm, char* buf);
char* ctime(const time_t* timep);
char* ctime_r(const time_t* timep, char* buf);
time_t mktime(struct tm* tm);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);
char* strptime(const char* buf, const char* format, struct tm* tm);
void tzset(void);

#ifdef __cplusplus
}
#endif

#endif
