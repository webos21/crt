#ifndef CRT_DL_H
#define CRT_DL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Format-neutral CRT extension. This deliberately does not fabricate ELF
 * program headers for Mach-O/PE. All fields are borrowed until callback return.
 * Enumeration is best-effort in the presence of concurrent module loading;
 * callbacks must not load/unload images. */
struct crt_dl_image {
  const char* path;
  const void* base;
  const char* format;
};
/* 0: completed, nonzero callback value: stopped, -1: error with errno.
 * Callbacks should use positive values for early termination. */
int crt_dl_iterate_images(int (*callback)(const struct crt_dl_image*, size_t, void*), void* data);

#ifdef __cplusplus
}
#endif
#endif
