#ifndef CRT_ANDROID_LOG_H
#define CRT_ANDROID_LOG_H

typedef enum android_LogPriority {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,
  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,
  ANDROID_LOG_SILENT,
} android_LogPriority;

typedef enum log_id {
  LOG_ID_MIN = 0,
  LOG_ID_MAIN = 0,
  LOG_ID_RADIO = 1,
  LOG_ID_EVENTS = 2,
  LOG_ID_SYSTEM = 3,
  LOG_ID_CRASH = 4,
  LOG_ID_STATS = 5,
  LOG_ID_SECURITY = 6,
  LOG_ID_KERNEL = 7,
  LOG_ID_MAX,
  LOG_ID_DEFAULT = 0x7FFFFFFF
} log_id_t;

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bionic's liblog entry points. CRT has no logd: a message is written to standard error as
 * "<P>/<tag>: <text>" (P is V, D, I, W, E or F), which is what `logcat -v brief` prints. */
int __android_log_write(int prio, const char* tag, const char* text);
int __android_log_print(int prio, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap);
void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...)
    __attribute__((noreturn, format(printf, 3, 4)));
int __android_log_buf_write(int bufID, int prio, const char* tag, const char* text);
int __android_log_buf_print(int bufID, int prio, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

#ifdef __cplusplus
}
#endif

#endif
