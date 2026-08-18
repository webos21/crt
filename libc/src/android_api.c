/* Bionic-compatible API-level policy for non-Android PAL hosts. */

#include <android/api-level.h>

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
