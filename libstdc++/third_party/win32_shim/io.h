/* Project-owned compatibility subset of <io.h> -- NOT a real Windows SDK/MSVC-CRT
 * header.
 *
 * libcxx's own src/filesystem/posix_compat.h does `#include <io.h>`
 * unconditionally under _LIBCPP_WIN32API, immediately followed by
 * `#include <windows.h>` (this same directory's own shim) in the same
 * preprocessor block -- confirmed by reading the fetched file directly.
 * _get_osfhandle() lives in this project's own windows.h shim (a real
 * implementation backed by
 * __crt_windows_fd_get_handle(), see that declaration's own comment in
 * windows.h for the fuller story) -- so this file only needs to exist
 * and intentionally contains only the two additional operations used by
 * libc++'s Windows filesystem implementation: _wopen() and _close().
 * They adapt Bionic-compatible UTF-32 wchar_t paths to this CRT's UTF-8
 * open() boundary; this does not import or expose an MSVC runtime ABI.
 */
#ifndef CRT_WIN32_SHIM_IO_H
#define CRT_WIN32_SHIM_IO_H

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

/* POSIX text and binary I/O are identical in this CRT.  libc++ passes this
 * MSVC flag while opening a binary copy_file descriptor, so represent it as
 * the deliberately no-op Bionic-compatible value. */
#ifndef O_BINARY
#define O_BINARY 0
#endif

static inline int _wopen(const wchar_t* path, int flags, ...) {
  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }

  size_t bytes = wcstombs(NULL, path, 0);
  if (bytes == (size_t)-1) {
    errno = EILSEQ;
    return -1;
  }
  char* utf8_path = (char*)malloc(bytes + 1);
  if (utf8_path == NULL) {
    errno = ENOMEM;
    return -1;
  }
  if (wcstombs(utf8_path, path, bytes + 1) == (size_t)-1) {
    free(utf8_path);
    errno = EILSEQ;
    return -1;
  }

  int result;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    int mode = va_arg(ap, int);
    va_end(ap);
    result = open(utf8_path, flags, mode);
  } else {
    result = open(utf8_path, flags);
  }
  free(utf8_path);
  return result;
}

static inline int _close(int fd) { return close(fd); }
#endif
