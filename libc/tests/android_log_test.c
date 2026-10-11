/* liblog entry points: formatting on standard error. Run with stderr merged into stdout (the
 * CTest PASS_REGULAR_EXPRESSION sees both). */
#include <android/log.h>
#include <stdio.h>

int main(void) {
  __android_log_print(ANDROID_LOG_INFO, "crtlog", "value=%d name=%s", 42, "wpe");
  __android_log_write(ANDROID_LOG_ERROR, "crtlog", "plain text");
  __android_log_buf_print(LOG_ID_MAIN, ANDROID_LOG_WARN, "crtlog", "buf %c", 'x');
  fflush(stderr);
  printf("android_log_test: ok\n");
  return 0;
}
