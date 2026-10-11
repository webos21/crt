/* Bionic-compatible API-level policy for non-Android PAL hosts. */

#include <android/api-level.h>
#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int android_get_application_target_sdk_version(void) {
  return __ANDROID_API_FUTURE__;
}

int android_get_device_api_level(void) {
  return __ANDROID_API_FUTURE__;
}

/* Bionic records this text in the process abort-message mapping for
 * tombstones. Linux/macOS/Windows PAL hosts do not have Android tombstones;
 * libc++abi has already emitted the same message to stderr before this hook. */
void android_set_abort_message(const char* message) {
  (void)message;
}

/* liblog: no logd on a PAL host, so a record is one line on standard error in `logcat -v brief`
 * form. Priorities below ANDROID_LOG_VERBOSE print as '?'; ANDROID_LOG_SILENT prints nothing. */
int __android_log_write(int prio, const char* tag, const char* text) {
  static const char letters[] = "??VDIWEF";
  char letter = '?';

  if (prio == ANDROID_LOG_SILENT) {
    return 0;
  }
  if (prio >= 0 && prio < ANDROID_LOG_SILENT) {
    letter = letters[prio];
  }
  return fprintf(stderr, "%c/%s: %s\n", letter, tag != 0 ? tag : "", text != 0 ? text : "") < 0 ? -1 : 1;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
  char buffer[1024];

  vsnprintf(buffer, sizeof(buffer), fmt != 0 ? fmt : "", ap);
  return __android_log_write(prio, tag, buffer);
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
  va_list ap;
  int result;

  va_start(ap, fmt);
  result = __android_log_vprint(prio, tag, fmt, ap);
  va_end(ap);
  return result;
}

int __android_log_buf_write(int bufID, int prio, const char* tag, const char* text) {
  (void)bufID;
  return __android_log_write(prio, tag, text);
}

int __android_log_buf_print(int bufID, int prio, const char* tag, const char* fmt, ...) {
  va_list ap;
  int result;

  (void)bufID;
  va_start(ap, fmt);
  result = __android_log_vprint(prio, tag, fmt, ap);
  va_end(ap);
  return result;
}

void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...) {
  char buffer[1024];

  if (fmt != 0) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
  } else {
    snprintf(buffer, sizeof(buffer), "Assertion failed: %s", cond != 0 ? cond : "");
  }
  __android_log_write(ANDROID_LOG_FATAL, tag, buffer);
  abort();
}
