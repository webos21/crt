#ifndef CRT_PLATFORM_H
#define CRT_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRT extensions, not additions to the Android/Bionic ABI. Strings describe
 * this runtime's target, not a cross compiler's build host. */
struct crt_platform_info {
  const char* os;
  const char* arch;
  const char* image_format;
  const char* executable_suffix;
  const char* shared_library_prefix;
  const char* shared_library_suffix;
};

const struct crt_platform_info* crt_get_platform_info(void);

/* Copy the user's absolute home path (HOME on Unix, USERPROFILE on Windows).
 * No directory is created; missing/relative values fail with ENOENT/EINVAL,
 * insufficient space with ERANGE. This is configuration, not authentication. */
int crt_get_user_home(char* buffer, size_t capacity);

/* 1: a native executable image for this target, 0: another format/target or
 * a script, -1: I/O/argument error with errno. Does not execute the file or
 * authenticate it, and is not a substitute for kernel loader validation. */
int crt_is_native_executable(const char* path);

#ifdef __cplusplus
}
#endif
#endif
