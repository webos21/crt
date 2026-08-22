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
 * and intentionally contains only the additional operations actually
 * used by consumers of this shim: _wopen()/_close() (libc++'s own
 * Windows filesystem implementation) and _wfopen() (Skia's own
 * src/ports/SkOSFile_stdio.cpp, confirmed for real 2026-08-22 via
 * `use of undeclared identifier '_wfopen'` -- a real MSVC-CRT function
 * distinct from _wopen: takes a wide *mode string* ("rb"/"wb"/...), not
 * an int flags bitmask, and returns FILE*, not an fd). They all adapt a
 * wide-character path (and, for _wfopen, mode string) to this CRT's
 * real narrow (UTF-8 on this target) open()/fopen() boundary via
 * wcstombs() -- confirmed for real (2026-08-22) that wchar_t is
 * genuinely 2 bytes (UTF-16) for this project's own real
 * x86_64-w64-mingw32 target (`__SIZEOF_WCHAR_T__` == 2), matching what
 * Skia's own caller already assumes (it builds a UTF-16 code-unit
 * buffer and casts it straight to wchar_t*); this file does not import
 * or expose any wider MSVC runtime ABI beyond that.
 */
#ifndef CRT_WIN32_SHIM_IO_H
#define CRT_WIN32_SHIM_IO_H

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
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

/* Shared by _wfopen() below: converts one NUL-terminated wide string to a
 * freshly malloc()'d narrow one, or NULL (with errno set) on failure.
 * Caller owns the result. */
static inline char* crt_win32_shim_wcs_to_narrow(const wchar_t* wide) {
  size_t bytes = wcstombs(NULL, wide, 0);
  if (bytes == (size_t)-1) {
    errno = EILSEQ;
    return NULL;
  }
  char* narrow = (char*)malloc(bytes + 1);
  if (narrow == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  if (wcstombs(narrow, wide, bytes + 1) == (size_t)-1) {
    free(narrow);
    errno = EILSEQ;
    return NULL;
  }
  return narrow;
}

static inline FILE* _wfopen(const wchar_t* path, const wchar_t* mode) {
  if (path == NULL || mode == NULL) {
    errno = EINVAL;
    return NULL;
  }

  char* narrow_path = crt_win32_shim_wcs_to_narrow(path);
  if (narrow_path == NULL) {
    return NULL;
  }
  char* narrow_mode = crt_win32_shim_wcs_to_narrow(mode);
  if (narrow_mode == NULL) {
    free(narrow_path);
    return NULL;
  }

  FILE* result = fopen(narrow_path, narrow_mode);
  free(narrow_path);
  free(narrow_mode);
  return result;
}
#endif
