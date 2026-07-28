#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

long __crt_sys_read(int fd, void* buf, unsigned long count);
long __crt_sys_write(int fd, const void* buf, unsigned long count);
long __crt_sys_open(const char* path, int flags, unsigned int mode);
long __crt_sys_close(int fd);
long long __crt_sys_lseek(int fd, long long offset, int whence);
long __crt_sys_access(const char* path, int mode);
long __crt_sys_mkdir(const char* path, unsigned int mode);
long __crt_sys_rmdir(const char* path);
long __crt_sys_chdir(const char* path);
#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_macos_fcntl(int fd, int cmd, void* arg);
#else
long __crt_sys_getcwd(char* buf, unsigned long size);
#endif
long __crt_sys_dup(int oldfd);
long __crt_sys_dup2(int oldfd, int newfd);
long __crt_sys_pipe(int pipefd[2]);
long __crt_sys_readlink(const char* path, char* buf, unsigned long size);
long __crt_sys_symlink(const char* target, const char* linkpath);
#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_statx(long dirfd, const char* path, int flags, unsigned int mask, void* statxbuf);
#elif defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_realpath_path(const char* path, char* resolved_path, unsigned long size);
long __crt_sys_stat_path(const char* path, struct stat* st);
long __crt_sys_lstat_path(const char* path, struct stat* st);
long __crt_sys_fstat(int fd, struct stat* st);
#elif defined(CRT_TARGET_OS_MACOS)
long __crt_sys_macos_stat64(const char* path, void* statbuf);
long __crt_sys_macos_fstat64(int fd, void* statbuf);
long __crt_sys_macos_lstat64(const char* path, void* statbuf);
#endif

#if defined(CRT_TARGET_OS_LINUX)
#define CRT_AT_FDCWD (-100L)
#define CRT_AT_EMPTY_PATH 0x1000
#define CRT_AT_SYMLINK_NOFOLLOW 0x0100
#define CRT_STATX_BASIC_STATS 0x000007ffU
#define CRT_STATX_ATTR_MODE 0x00000002U
#define CRT_STATX_ATTR_NLINK 0x00000004U
#define CRT_STATX_ATTR_UID 0x00000008U
#define CRT_STATX_ATTR_GID 0x00000010U
#define CRT_STATX_ATTR_ATIME 0x00000020U
#define CRT_STATX_ATTR_MTIME 0x00000040U
#define CRT_STATX_ATTR_CTIME 0x00000080U
#define CRT_STATX_ATTR_INO 0x00000100U
#define CRT_STATX_ATTR_SIZE 0x00000200U
#define CRT_STATX_ATTR_BLOCKS 0x00000400U

struct crt_statx_timestamp {
  int64_t tv_sec;
  uint32_t tv_nsec;
  int32_t reserved;
};

struct crt_statx {
  uint32_t mask;
  uint32_t blksize;
  uint64_t attributes;
  uint32_t nlink;
  uint32_t uid;
  uint32_t gid;
  uint16_t mode;
  uint16_t reserved0;
  uint64_t ino;
  uint64_t size;
  uint64_t blocks;
  uint64_t attributes_mask;
  struct crt_statx_timestamp atime;
  struct crt_statx_timestamp btime;
  struct crt_statx_timestamp ctime;
  struct crt_statx_timestamp mtime;
  uint32_t rdev_major;
  uint32_t rdev_minor;
  uint32_t dev_major;
  uint32_t dev_minor;
  uint64_t spare[14];
};
#endif

#if defined(CRT_TARGET_OS_MACOS)
struct crt_darwin_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

struct crt_darwin_stat64 {
  int32_t dev;
  uint16_t mode;
  uint16_t nlink;
  uint64_t ino;
  uint32_t uid;
  uint32_t gid;
  int32_t rdev;
  int32_t padding0;
  struct crt_darwin_timespec atime;
  struct crt_darwin_timespec mtime;
  struct crt_darwin_timespec ctime;
  struct crt_darwin_timespec birthtime;
  int64_t size;
  int64_t blocks;
  int32_t blksize;
  uint32_t flags;
  uint32_t gen;
  int32_t lspare;
  int64_t qspare[2];
};

typedef char crt_darwin_stat64_size_check[
    sizeof(struct crt_darwin_stat64) == 144 ? 1 : -1];

#define CRT_MACOS_F_GETPATH 50
#define CRT_MACOS_MAXPATHLEN 1024
#endif

static long normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return result;
}

static int path_is_absolute(const char* path) {
  if (path == 0 || path[0] == 0) {
    return 0;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
    return path[1] == ':';
  }
  return (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\');
#else
  return path[0] == '/';
#endif
}

static int path_separator(int c) {
#if defined(CRT_TARGET_OS_WINDOWS)
  return c == '/' || c == '\\';
#else
  return c == '/';
#endif
}

static size_t path_root_length(const char* path) {
#if defined(CRT_TARGET_OS_WINDOWS)
  if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':') {
    return path_separator(path[2]) ? 3 : 2;
  }
  if (path_separator(path[0]) && path_separator(path[1])) {
    return 2;
  }
#endif
  return path_separator(path[0]) ? 1 : 0;
}

static int normalize_absolute_path(const char* input, char* output, size_t output_size) {
  size_t root;
  size_t in_pos;
  size_t out_pos;

  if (input == 0 || output == 0 || output_size == 0) {
    return (int)__set_errno(EINVAL);
  }
  root = path_root_length(input);
  if (root == 0 || root >= output_size) {
    return (int)__set_errno(EINVAL);
  }
  memcpy(output, input, root);
  out_pos = root;
  in_pos = root;

  while (path_separator(input[in_pos])) {
    ++in_pos;
  }
  while (input[in_pos] != 0) {
    size_t part_start = in_pos;
    size_t part_len;

    while (input[in_pos] != 0 && !path_separator(input[in_pos])) {
      ++in_pos;
    }
    part_len = in_pos - part_start;
    while (path_separator(input[in_pos])) {
      ++in_pos;
    }
    if (part_len == 0 || (part_len == 1 && input[part_start] == '.')) {
      continue;
    }
    if (part_len == 2 && input[part_start] == '.' && input[part_start + 1] == '.') {
      if (out_pos > root) {
        --out_pos;
        while (out_pos > root && !path_separator(output[out_pos - 1])) {
          --out_pos;
        }
      }
      continue;
    }
    if (out_pos > root && !path_separator(output[out_pos - 1])) {
      if (out_pos + 1 >= output_size) {
        return (int)__set_errno(ERANGE);
      }
      output[out_pos++] = '/';
    }
    if (out_pos + part_len >= output_size) {
      return (int)__set_errno(ERANGE);
    }
    memcpy(output + out_pos, input + part_start, part_len);
    out_pos += part_len;
  }
  if (out_pos == 0) {
    return (int)__set_errno(EINVAL);
  }
  if (out_pos > 1 && path_separator(output[out_pos - 1])) {
    --out_pos;
  }
  output[out_pos] = 0;
  return 0;
}

ssize_t read(int fd, void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_read(fd, buf, (unsigned long)count));
}

ssize_t write(int fd, const void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_write(fd, buf, (unsigned long)count));
}

int open(const char* path, int flags, ...) {
  unsigned int mode = 0;
  va_list args;

  if ((flags & O_CREAT) != 0) {
    va_start(args, flags);
    mode = (unsigned int)va_arg(args, int);
    va_end(args);
  }

  return (int)normalize_syscall_result(__crt_sys_open(path, flags, mode));
}

int close(int fd) {
  return (int)normalize_syscall_result(__crt_sys_close(fd));
}

off_t lseek(int fd, off_t offset, int whence) {
  long long result = __crt_sys_lseek(fd, (long long)offset, whence);
  if (result < 0 && result >= -4095) {
    return (off_t)__set_errno((int)-result);
  }
  return (off_t)result;
}

int access(const char* path, int mode) {
  if (path == 0 || (mode & ~(R_OK | W_OK | X_OK)) != 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_access(path, mode));
}

int mkdir(const char* path, mode_t mode) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_mkdir(path, (unsigned int)mode));
}

int rmdir(const char* path) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_rmdir(path));
}

int chdir(const char* path) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_chdir(path));
}

char* getcwd(char* buf, size_t size) {
#if defined(CRT_TARGET_OS_MACOS)
  char path[CRT_MACOS_MAXPATHLEN];
  size_t length;
  int fd;
  long result;

  if (buf == 0 || size == 0) {
    __set_errno(EINVAL);
    return 0;
  }
  fd = open(".", O_RDONLY);
  if (fd < 0) {
    return 0;
  }
  memset(path, 0, sizeof(path));
  result = __crt_sys_macos_fcntl(fd, CRT_MACOS_F_GETPATH, path);
  close(fd);
  if (result < 0 && result >= -4095) {
    __set_errno((int)-result);
    return 0;
  }
  length = strlen(path);
  if (length + 1 > size) {
    __set_errno(ERANGE);
    return 0;
  }
  memcpy(buf, path, length + 1);
  return buf;
#else
  long result;

  if (buf == 0 || size == 0) {
    __set_errno(EINVAL);
    return 0;
  }
  result = __crt_sys_getcwd(buf, (unsigned long)size);
  if (result < 0 && result >= -4095) {
    __set_errno((int)-result);
    return 0;
  }
  return buf;
#endif
}

char* realpath(const char* path, char* resolved_path) {
  char combined[PATH_MAX];
  char cwd[PATH_MAX];
  char* output = resolved_path;
  size_t cwd_len;
  size_t path_len;
  struct stat st;

  if (path == 0) {
    __set_errno(EINVAL);
    return 0;
  }
  if (stat(path, &st) != 0) {
    return 0;
  }
  if (output == 0) {
    output = (char*)malloc(PATH_MAX);
    if (output == 0) {
      __set_errno(ENOMEM);
      return 0;
    }
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  {
    char absolute[PATH_MAX];

    if (__crt_sys_realpath_path(path, absolute, sizeof(absolute)) == 0) {
      if (normalize_absolute_path(absolute, output, PATH_MAX) == 0) {
        return output;
      }
      if (resolved_path == 0) {
        free(output);
      }
      return 0;
    }
  }
#endif

  if (path_is_absolute(path)) {
    if (normalize_absolute_path(path, output, PATH_MAX) != 0) {
      if (resolved_path == 0) {
        free(output);
      }
      return 0;
    }
    return output;
  }

  if (getcwd(cwd, sizeof(cwd)) == 0) {
    if (resolved_path == 0) {
      free(output);
    }
    return 0;
  }
  cwd_len = strlen(cwd);
  path_len = strlen(path);
  if (cwd_len + 1 + path_len + 1 > sizeof(combined)) {
    if (resolved_path == 0) {
      free(output);
    }
    __set_errno(ERANGE);
    return 0;
  }
  memcpy(combined, cwd, cwd_len);
  combined[cwd_len] = '/';
  memcpy(combined + cwd_len + 1, path, path_len + 1);
  if (normalize_absolute_path(combined, output, PATH_MAX) != 0) {
    if (resolved_path == 0) {
      free(output);
    }
    return 0;
  }
  return output;
}

ssize_t readlink(const char* path, char* buf, size_t bufsiz) {
  if (path == 0 || buf == 0) {
    return (ssize_t)__set_errno(EINVAL);
  }
  return (ssize_t)normalize_syscall_result(__crt_sys_readlink(path, buf, (unsigned long)bufsiz));
}

int symlink(const char* target, const char* linkpath) {
  if (target == 0 || linkpath == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_symlink(target, linkpath));
}

int dup(int oldfd) {
  return (int)normalize_syscall_result(__crt_sys_dup(oldfd));
}

int dup2(int oldfd, int newfd) {
  if (newfd < 0) {
    return (int)__set_errno(EBADF);
  }
  return (int)normalize_syscall_result(__crt_sys_dup2(oldfd, newfd));
}

int pipe(int pipefd[2]) {
  if (pipefd == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_pipe(pipefd));
}

int isatty(int fd) {
  struct stat st;

  if (fstat(fd, &st) != 0) {
    return 0;
  }
  return S_ISCHR(st.st_mode);
}

int fcntl(int fd, int cmd, ...) {
  va_list args;
  int arg;
  int copy;
  int saved[64];
  int saved_count = 0;
  int result;

  switch (cmd) {
    case F_DUPFD:
      va_start(args, cmd);
      arg = va_arg(args, int);
      va_end(args);
      if (arg < 0) {
        return (int)__set_errno(EINVAL);
      }
      do {
        copy = dup(fd);
        if (copy < 0) {
          while (saved_count > 0) {
            close(saved[--saved_count]);
          }
          return -1;
        }
        if (copy >= arg) {
          result = copy;
          while (saved_count > 0) {
            close(saved[--saved_count]);
          }
          return result;
        }
        if (saved_count == (int)(sizeof(saved) / sizeof(saved[0]))) {
          close(copy);
          while (saved_count > 0) {
            close(saved[--saved_count]);
          }
          return (int)__set_errno(EMFILE);
        }
        saved[saved_count++] = copy;
      } while (1);

    case F_GETFD:
      if (fstat(fd, &(struct stat){0}) != 0) {
        return -1;
      }
      return 0;

    case F_SETFD:
      va_start(args, cmd);
      arg = va_arg(args, int);
      va_end(args);
      if ((arg & ~FD_CLOEXEC) != 0) {
        return (int)__set_errno(EINVAL);
      }
      if (fstat(fd, &(struct stat){0}) != 0) {
        return -1;
      }
      return 0;

    case F_GETFL:
      if (fstat(fd, &(struct stat){0}) != 0) {
        return -1;
      }
      return O_RDWR;

    case F_SETFL:
      va_start(args, cmd);
      (void)va_arg(args, int);
      va_end(args);
      if (fstat(fd, &(struct stat){0}) != 0) {
        return -1;
      }
      return 0;

    default:
      return (int)__set_errno(EINVAL);
  }
}

#if !defined(CRT_TARGET_OS_LINUX) && !defined(CRT_TARGET_OS_WINDOWS) && !defined(CRT_TARGET_OS_MACOS)
static int fallback_fstat(int fd, struct stat* st) {
  off_t current;
  off_t end;

  if (st == 0) {
    return (int)__set_errno(EINVAL);
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  st->st_nlink = 1;
  st->st_blksize = 4096;
  current = lseek(fd, 0, SEEK_CUR);
  end = lseek(fd, 0, SEEK_END);
  if (end < 0) {
    return -1;
  }
  st->st_size = end;
  st->st_blocks = (end + 511) / 512;
  if (current >= 0) {
    (void)lseek(fd, current, SEEK_SET);
  }
  return 0;
}

static int fallback_stat(const char* path, struct stat* st) {
  int fd;

  if (path == 0 || st == 0) {
    return (int)__set_errno(EINVAL);
  }
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  if (fallback_fstat(fd, st) != 0) {
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}
#endif

#if defined(CRT_TARGET_OS_MACOS)
static int darwin_stat_to_stat(const struct crt_darwin_stat64* ds, struct stat* st) {
  memset(st, 0, sizeof(*st));
  st->st_dev = (dev_t)(uint32_t)ds->dev;
  st->st_ino = (ino_t)ds->ino;
  st->st_mode = ds->mode;
  st->st_nlink = ds->nlink;
  st->st_uid = ds->uid;
  st->st_gid = ds->gid;
  st->st_rdev = (dev_t)(uint32_t)ds->rdev;
  st->st_size = (off_t)ds->size;
  st->st_blksize = (blksize_t)ds->blksize;
  st->st_blocks = (blkcnt_t)ds->blocks;
  st->st_atime = (time_t)ds->atime.tv_sec;
  st->st_mtime = (time_t)ds->mtime.tv_sec;
  st->st_ctime = (time_t)ds->ctime.tv_sec;
  return 0;
}

static int macos_stat64_result(long result, const struct crt_darwin_stat64* ds, struct stat* st) {
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  return darwin_stat_to_stat(ds, st);
}
#endif

#if defined(CRT_TARGET_OS_LINUX)
static int statx_to_stat(const struct crt_statx* sx, struct stat* st) {
  memset(st, 0, sizeof(*st));
  if ((sx->mask & CRT_STATX_ATTR_MODE) != 0) {
    st->st_mode = sx->mode;
  }
  if ((sx->mask & CRT_STATX_ATTR_NLINK) != 0) {
    st->st_nlink = sx->nlink;
  }
  if ((sx->mask & CRT_STATX_ATTR_UID) != 0) {
    st->st_uid = sx->uid;
  }
  if ((sx->mask & CRT_STATX_ATTR_GID) != 0) {
    st->st_gid = sx->gid;
  }
  if ((sx->mask & CRT_STATX_ATTR_INO) != 0) {
    st->st_ino = sx->ino;
  }
  if ((sx->mask & CRT_STATX_ATTR_SIZE) != 0) {
    st->st_size = (off_t)sx->size;
  }
  if ((sx->mask & CRT_STATX_ATTR_BLOCKS) != 0) {
    st->st_blocks = (blkcnt_t)sx->blocks;
  }
  if ((sx->mask & CRT_STATX_ATTR_ATIME) != 0) {
    st->st_atime = (time_t)sx->atime.tv_sec;
  }
  if ((sx->mask & CRT_STATX_ATTR_MTIME) != 0) {
    st->st_mtime = (time_t)sx->mtime.tv_sec;
  }
  if ((sx->mask & CRT_STATX_ATTR_CTIME) != 0) {
    st->st_ctime = (time_t)sx->ctime.tv_sec;
  }
  st->st_dev = ((uint64_t)sx->dev_major << 32) | sx->dev_minor;
  st->st_rdev = ((uint64_t)sx->rdev_major << 32) | sx->rdev_minor;
  st->st_blksize = sx->blksize != 0 ? (blksize_t)sx->blksize : 4096;
  return 0;
}

static int linux_statx(long dirfd, const char* path, int flags, struct stat* st) {
  struct crt_statx sx;
  long result;

  if (st == 0 || path == 0) {
    return (int)__set_errno(EINVAL);
  }
  memset(&sx, 0, sizeof(sx));
  result = __crt_sys_statx(dirfd, path, flags, CRT_STATX_BASIC_STATS, &sx);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  return statx_to_stat(&sx, st);
}
#endif

int stat(const char* path, struct stat* st) {
#if defined(CRT_TARGET_OS_LINUX)
  return linux_statx(CRT_AT_FDCWD, path, 0, st);
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (path == 0 || st == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_stat_path(path, st));
#elif defined(CRT_TARGET_OS_MACOS)
  {
    struct crt_darwin_stat64 ds;
    if (path == 0 || st == 0) {
      return (int)__set_errno(EINVAL);
    }
    memset(&ds, 0, sizeof(ds));
    return macos_stat64_result(__crt_sys_macos_stat64(path, &ds), &ds, st);
  }
#else
  return fallback_stat(path, st);
#endif
}

int lstat(const char* path, struct stat* st) {
#if defined(CRT_TARGET_OS_LINUX)
  return linux_statx(CRT_AT_FDCWD, path, CRT_AT_SYMLINK_NOFOLLOW, st);
#elif defined(CRT_TARGET_OS_MACOS)
  {
    struct crt_darwin_stat64 ds;
    if (path == 0 || st == 0) {
      return (int)__set_errno(EINVAL);
    }
    memset(&ds, 0, sizeof(ds));
    return macos_stat64_result(__crt_sys_macos_lstat64(path, &ds), &ds, st);
  }
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (path == 0 || st == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_lstat_path(path, st));
#else
  return stat(path, st);
#endif
}

int fstat(int fd, struct stat* st) {
#if defined(CRT_TARGET_OS_LINUX)
  return linux_statx((long)fd, "", CRT_AT_EMPTY_PATH, st);
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (st == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_fstat(fd, st));
#elif defined(CRT_TARGET_OS_MACOS)
  {
    struct crt_darwin_stat64 ds;
    if (st == 0) {
      return (int)__set_errno(EINVAL);
    }
    memset(&ds, 0, sizeof(ds));
    return macos_stat64_result(__crt_sys_macos_fstat64(fd, &ds), &ds, st);
  }
#else
  return fallback_fstat(fd, st);
#endif
}
