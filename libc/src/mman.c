#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

void* __crt_sys_mmap(void* addr, unsigned long length, int prot, int flags, int fd, long long offset);
long __crt_sys_mprotect(void* addr, unsigned long length, int prot);
long __crt_sys_munmap(void* addr, unsigned long length);
long __crt_sys_msync(void* addr, unsigned long length, int flags);
void* __crt_sys_mremap(void* old_addr, unsigned long old_size, unsigned long new_size, int flags, void* new_addr);
long __crt_sys_mlockall(int flags);
long __crt_sys_munlockall(void);
long __crt_sys_mlock(const void* addr, unsigned long length);
long __crt_sys_mlock2(const void* addr, unsigned long length, int flags);
long __crt_sys_munlock(const void* addr, unsigned long length);
long __crt_sys_mincore(void* addr, unsigned long length, unsigned char* vector);
long __crt_sys_madvise(void* addr, unsigned long length, int advice);

static int invalid_length(size_t length) {
  if (length == 0) {
    errno = EINVAL;
    return 1;
  }
  return 0;
}

static long normalize_long(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

static int host_mmap_flags(int flags) {
#if defined(CRT_TARGET_OS_MACOS)
  int host_flags = flags;

  host_flags &= ~(MAP_GROWSDOWN | MAP_DENYWRITE | MAP_EXECUTABLE | MAP_LOCKED |
                  MAP_POPULATE | MAP_NONBLOCK | MAP_STACK | MAP_HUGETLB);
  if ((host_flags & MAP_ANONYMOUS) != 0) {
    host_flags &= ~MAP_ANONYMOUS;
    host_flags |= 0x1000;
  }
  if ((host_flags & MAP_NORESERVE) != 0) {
    host_flags &= ~MAP_NORESERVE;
    host_flags |= 0x0040;
  }
  return host_flags;
#else
  return flags;
#endif
}

void* mmap64(void* addr, size_t length, int prot, int flags, int fd, off64_t offset) {
  void* result;
  intptr_t value;

  if (invalid_length(length)) {
    return MAP_FAILED;
  }

  result = __crt_sys_mmap(addr, (unsigned long)length, prot, host_mmap_flags(flags), fd, (long long)offset);
  value = (intptr_t)result;
  if (value < 0 && value >= -4095) {
    errno = (int)-value;
    return MAP_FAILED;
  }
  return result;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  return mmap64(addr, length, prot, flags, fd, (off64_t)offset);
}

int munmap(void* addr, size_t length) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }

  result = __crt_sys_munmap(addr, (unsigned long)length);
  return (int)normalize_long(result);
}

int mprotect(void* addr, size_t length, int prot) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }

  result = __crt_sys_mprotect(addr, (unsigned long)length, prot);
  return (int)normalize_long(result);
}

int msync(void* addr, size_t length, int flags) {
  long result;

  if (invalid_length(length)) {
    return -1;
  }
  if ((flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC)) != 0 || ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_msync(addr, (unsigned long)length, flags);
  return (int)normalize_long(result);
}

void* mremap(void* old_addr, size_t old_size, size_t new_size, int flags, ...) {
  void* new_addr = 0;
  void* result;
  intptr_t value;

  if (old_size == 0 || new_size == 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  if ((flags & MREMAP_FIXED) != 0) {
    va_list ap;
    va_start(ap, flags);
    new_addr = va_arg(ap, void*);
    va_end(ap);
  }

  result = __crt_sys_mremap(old_addr, (unsigned long)old_size, (unsigned long)new_size, flags, new_addr);
  value = (intptr_t)result;
  if (value < 0 && value >= -4095) {
    errno = (int)-value;
    return MAP_FAILED;
  }
  return result;
}

int mlockall(int flags) {
  long result;

  if ((flags & ~(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT)) != 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_mlockall(flags);
  return (int)normalize_long(result);
}

int munlockall(void) {
  return (int)normalize_long(__crt_sys_munlockall());
}

int mlock(const void* addr, size_t length) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_mlock(addr, (unsigned long)length));
}

int mlock2(const void* addr, size_t length, int flags) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_mlock2(addr, (unsigned long)length, flags));
}

int munlock(const void* addr, size_t length) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_munlock(addr, (unsigned long)length));
}

int mincore(void* addr, size_t length, unsigned char* vector) {
  if (invalid_length(length)) {
    return -1;
  }
  if (vector == 0) {
    errno = EFAULT;
    return -1;
  }
  return (int)normalize_long(__crt_sys_mincore(addr, (unsigned long)length, vector));
}

int madvise(void* addr, size_t length, int advice) {
  if (invalid_length(length)) {
    return -1;
  }
  return (int)normalize_long(__crt_sys_madvise(addr, (unsigned long)length, advice));
}

int posix_madvise(void* addr, size_t length, int advice) {
  long result;

  if (length == 0) {
    return 0;
  }
  result = __crt_sys_madvise(addr, (unsigned long)length, advice);
  if (result < 0 && result >= -4095) {
    return (int)-result;
  }
  return (int)result;
}

/* A real, known-writable directory for memfd_create()'s own temp-file
 * trick below -- NOT a bare relative path in whatever the caller's own
 * current working directory happens to be, and not a bare hardcoded
 * "/tmp" either (a real regression found and fixed the same day this
 * function's own directory choice was first changed away from CWD: bare
 * "/tmp" does not exist on this project's own Windows host at all --
 * confirmed directly via memfd_create_test.exe there, ENOENT). Checks
 * $TMPDIR first (the real POSIX convention, matching get_tmpdir() in
 * stdio.c's own tmpnam()/tempnam() -- not reused directly since it is
 * static to that file, but the same real precedent), then $TEMP/$TMP
 * (Windows' own real per-process environment, always set by a real
 * Windows session), then a final "/tmp" fallback for a POSIX host with
 * none of those set (matches this same function's own most recent, only
 * previously-tested environment, WSL, which really does have a real
 * native-filesystem "/tmp"). */
static const char* memfd_tmpdir(void) {
  const char* dir = getenv("TMPDIR");
  if (dir != 0 && dir[0] != '\0') {
    return dir;
  }
  dir = getenv("TEMP");
  if (dir != 0 && dir[0] != '\0') {
    return dir;
  }
  dir = getenv("TMP");
  if (dir != 0 && dir[0] != '\0') {
    return dir;
  }
  return "/tmp";
}

int memfd_create(const char* name, unsigned int flags) {
  char path[4096];
  static unsigned long counter;
  unsigned long attempt;

  (void)name; /* Real Linux memfd_create()'s name argument is purely a
               * debug label visible in /proc/self/fd/N's symlink target --
               * it has no functional effect on the fd itself, so there's
               * nothing meaningful to do with it here. */

  if ((flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING)) != 0) {
    errno = EINVAL;
    return -1;
  }

  for (attempt = 0; attempt < 1000; ++attempt) {
    unsigned long value = counter++;
    int fd;

    /* A real, confirmed-live bug (2026-09-02, root-caused via strace, not
     * guessed): this project's own libcrtgfx Linux Wayland backend calls
     * memfd_create() from crtgfx_host_window_present_software() with the
     * caller's CWD often sitting on whatever filesystem the whole build/
     * test tree happens to be on -- on WSL specifically, that is very
     * often `/mnt/c/...` (DrvFs, a 9p-protocol bridge to the real Windows
     * NTFS volume, confirmed via `mount`), which does NOT correctly
     * preserve this function's own core trick (open() a real file,
     * unlink() it immediately, keep using the now-nameless fd -- ordinary
     * "delete-while-open" semantics every real Unix-native filesystem,
     * ext4 included, honors correctly). Confirmed directly via strace:
     * open()+unlink() both succeeded, but the very next ftruncate() on
     * the resulting fd failed with ENOENT -- DrvFs orphans the underlying
     * file once its last directory entry is removed instead of keeping
     * it reachable through the still-open fd the way a native filesystem
     * does. This surfaced as `crtgfx_window_smoke`'s own real end_frame()
     * failure (CRTGFX_ERROR_HOST) on every WSL run, previously
     * mis-attributed (see this project's own git history) to "no
     * reachable Wayland compositor" -- a real WSLg connection was present
     * the whole time; the actual break was this function's own filesystem
     * assumption, not display connectivity. memfd_tmpdir() (above) fixes
     * this by using a real, known-good, non-CWD-relative directory
     * instead. */
    int written = snprintf(path, sizeof(path), "%s/crt_memfd_%d_%06lu.tmp", memfd_tmpdir(), (int)getpid(),
                            value % 1000000UL);
    if (written < 0 || (size_t)written >= sizeof(path)) {
      errno = ENAMETOOLONG;
      return -1;
    }

    fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      return -1;
    }
    if (unlink(path) != 0) {
      /* Shouldn't happen on a file just created, but don't hand back an fd
       * still visibly named on the filesystem if it somehow does. */
      int saved_errno = errno;

      close(fd);
      errno = saved_errno;
      return -1;
    }
    if ((flags & MFD_CLOEXEC) != 0 && fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
      int saved_errno = errno;

      close(fd);
      errno = saved_errno;
      return -1;
    }
    return fd;
  }
  errno = EEXIST;
  return -1;
}
