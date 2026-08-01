#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <private/crt_fd_table.h>

#ifndef TMP_MAX
#define TMP_MAX 308915776
#endif

long __crt_sys_read(int fd, void* buf, unsigned long count);
long __crt_sys_write(int fd, const void* buf, unsigned long count);
long __crt_sys_fcntl(int fd, int cmd, void* arg);
long __crt_sys_ioctl(int fd, unsigned long request, void* arg);
long __crt_sys_pread(int fd, void* buf, unsigned long count, long long offset);
long __crt_sys_pwrite(int fd, const void* buf, unsigned long count, long long offset);
long __crt_sys_open(const char* path, int flags, unsigned int mode);
long __crt_sys_close(int fd);
long long __crt_sys_lseek(int fd, long long offset, int whence);
long __crt_sys_ftruncate(int fd, long long length);
long __crt_sys_fsync(int fd);
long __crt_sys_access(const char* path, int mode);
long __crt_sys_mkdir(const char* path, unsigned int mode);
long __crt_sys_rmdir(const char* path);
long __crt_sys_chdir(const char* path);
long __crt_sys_chmod(const char* path, unsigned int mode);
long __crt_sys_fchmod(int fd, unsigned int mode);
#if !defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_umask(unsigned int mask);
#endif
#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_macos_fcntl(int fd, int cmd, void* arg);
#else
long __crt_sys_getcwd(char* buf, unsigned long size);
#endif
long __crt_sys_dup(int oldfd);
long __crt_sys_dup2(int oldfd, int newfd);
long __crt_sys_pipe(int pipefd[2]);
long __crt_sys_unlink(const char* path);
long __crt_sys_readlink(const char* path, char* buf, unsigned long size);
long __crt_sys_symlink(const char* target, const char* linkpath);
long __crt_sys_geteuid(void);
long __crt_sys_fchown(int fd, unsigned int owner, unsigned int group);
long __crt_sys_statfs(const char* path, struct statfs* buf);
long __crt_sys_fstatfs(int fd, struct statfs* buf);
#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_statx(long dirfd, const char* path, int flags, unsigned int mask, void* statxbuf);
#elif defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_realpath_path(const char* path, char* resolved_path, unsigned long size);
long __crt_sys_isatty(int fd);
long __crt_sys_stat_path(const char* path, struct stat* st);
long __crt_sys_lstat_path(const char* path, struct stat* st);
long __crt_sys_fstat(int fd, struct stat* st);
#elif defined(CRT_TARGET_OS_MACOS)
long __crt_sys_macos_stat64(const char* path, void* statbuf);
long __crt_sys_macos_fstat64(int fd, void* statbuf);
long __crt_sys_macos_lstat64(const char* path, void* statbuf);
long __crt_sys_macos_statfs64(const char* path, void* statfsbuf);
long __crt_sys_macos_fstatfs64(int fd, void* statfsbuf);
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

struct crt_darwin_statfs64 {
  uint32_t f_bsize;
  int32_t f_iosize;
  uint64_t f_blocks;
  uint64_t f_bfree;
  uint64_t f_bavail;
  uint64_t f_files;
  uint64_t f_ffree;
  fsid_t f_fsid;
  uint32_t f_owner;
  uint32_t f_type;
  uint32_t f_flags;
  uint32_t f_fssubtype;
  char f_fstypename[16];
  char f_mntonname[1024];
  char f_mntfromname[1024];
  uint32_t f_flags_ext;
  uint32_t f_reserved[7];
};

struct crt_darwin_flock {
  int64_t l_start;
  int64_t l_len;
  int32_t l_pid;
  int16_t l_type;
  int16_t l_whence;
};

#define CRT_DARWIN_F_GETLK 7
#define CRT_DARWIN_F_SETLK 8
#define CRT_DARWIN_F_SETLKW 9
#define CRT_DARWIN_F_RDLCK 1
#define CRT_DARWIN_F_UNLCK 2
#define CRT_DARWIN_F_WRLCK 3
#define CRT_DARWIN_FIONREAD 0x4004667fUL
#define CRT_DARWIN_TIOCGPGRP 0x40047477UL
#define CRT_DARWIN_TIOCSPGRP 0x80047476UL
#define CRT_DARWIN_TIOCGWINSZ 0x40087468UL
#define CRT_DARWIN_TIOCSWINSZ 0x80087467UL

static long macos_fcntl_lock(int fd, int cmd, struct flock* lock);
static long macos_ioctl(int fd, unsigned long request, void* arg);

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

static char fd_snapshot_hex_digit(unsigned int value) {
  return (char)(value < 10 ? '0' + value : 'a' + (value - 10));
}

static int fd_snapshot_hex_value(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static int fd_snapshot_append_hex(char* buffer, unsigned long size, unsigned long* pos, uintptr_t value) {
  char digits[sizeof(uintptr_t) * 2];
  unsigned int count = 0;

  do {
    digits[count++] = fd_snapshot_hex_digit((unsigned int)(value & 0xfU));
    value >>= 4;
  } while (value != 0);
  while (count > 0) {
    if (*pos + 1 >= size) {
      return ENOSPC;
    }
    buffer[(*pos)++] = digits[--count];
  }
  buffer[*pos] = 0;
  return 0;
}

static int fd_snapshot_parse_hex(const char** cursor, uintptr_t* value) {
  uintptr_t parsed = 0;
  int digits = 0;

  while (**cursor != 0 && **cursor != '|' && **cursor != ':' && **cursor != ';') {
    int digit = fd_snapshot_hex_value((unsigned char)**cursor);

    if (digit < 0) {
      return EINVAL;
    }
    parsed = (parsed << 4) | (uintptr_t)digit;
    ++*cursor;
    ++digits;
  }
  if (digits == 0) {
    return EINVAL;
  }
  *value = parsed;
  return 0;
}

static int fd_snapshot_append_bytes(
    char* buffer,
    unsigned long size,
    unsigned long* pos,
    const unsigned char* bytes,
    unsigned int count) {
  static const char hex[] = "0123456789abcdef";
  unsigned int i;

  if (*pos + (unsigned long)count * 2UL >= size) {
    return ENOSPC;
  }
  for (i = 0; i < count; ++i) {
    buffer[(*pos)++] = hex[(bytes[i] >> 4) & 0xfU];
    buffer[(*pos)++] = hex[bytes[i] & 0xfU];
  }
  buffer[*pos] = 0;
  return 0;
}

static int fd_snapshot_parse_bytes(
    const char** cursor,
    unsigned char* bytes,
    unsigned int count) {
  unsigned int i;

  for (i = 0; i < count; ++i) {
    int high = fd_snapshot_hex_value((unsigned char)(*cursor)[0]);
    int low = fd_snapshot_hex_value((unsigned char)(*cursor)[1]);

    if (high < 0 || low < 0) {
      return EINVAL;
    }
    bytes[i] = (unsigned char)((high << 4) | low);
    *cursor += 2;
  }
  return 0;
}

int __crt_fd_snapshot_encode(const struct crt_fd_snapshot* snapshot, char* buffer, unsigned long size) {
  unsigned long pos = 0;
  unsigned int i;
  int result;

  if (snapshot == 0 || buffer == 0 || size == 0 ||
      snapshot->magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot->version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot->count > snapshot->capacity ||
      snapshot->capacity > CRT_FD_SNAPSHOT_MAX) {
    return EINVAL;
  }
  buffer[0] = 0;
  result = fd_snapshot_append_hex(buffer, size, &pos, snapshot->version);
  if (result != 0) {
    return result;
  }
  if (pos + 1 >= size) {
    return ENOSPC;
  }
  buffer[pos++] = '|';
  buffer[pos] = 0;
  for (i = 0; i < snapshot->count; ++i) {
    const struct crt_fd_snapshot_entry* entry = &snapshot->entries[i];

    result = fd_snapshot_append_hex(buffer, size, &pos, (uintptr_t)entry->fd);
    if (result != 0 || pos + 1 >= size) {
      return result != 0 ? result : ENOSPC;
    }
    buffer[pos++] = ':';
    result = fd_snapshot_append_hex(buffer, size, &pos, (uintptr_t)entry->kind);
    if (result != 0 || pos + 1 >= size) {
      return result != 0 ? result : ENOSPC;
    }
    buffer[pos++] = ':';
    result = fd_snapshot_append_hex(buffer, size, &pos, (uintptr_t)entry->flags);
    if (result != 0 || pos + 1 >= size) {
      return result != 0 ? result : ENOSPC;
    }
    buffer[pos++] = ':';
    result = fd_snapshot_append_hex(buffer, size, &pos, entry->handle);
    if (result != 0 || pos + 1 >= size) {
      return result != 0 ? result : ENOSPC;
    }
    if (entry->socket_protocol_info_size != 0) {
      buffer[pos++] = ':';
      result = fd_snapshot_append_hex(
          buffer, size, &pos, (uintptr_t)entry->socket_protocol_info_size);
      if (result != 0 || pos + 1 >= size) {
        return result != 0 ? result : ENOSPC;
      }
      buffer[pos++] = ':';
      result = fd_snapshot_append_bytes(
          buffer,
          size,
          &pos,
          entry->socket_protocol_info,
          entry->socket_protocol_info_size);
      if (result != 0 || pos + 1 >= size) {
        return result != 0 ? result : ENOSPC;
      }
    }
    buffer[pos++] = ';';
    buffer[pos] = 0;
  }
  return 0;
}

int __crt_fd_snapshot_decode(const char* text, struct crt_fd_snapshot* snapshot) {
  const char* cursor = text;
  uintptr_t version = 0;
  int result;

  if (text == 0 || snapshot == 0) {
    return EINVAL;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  result = fd_snapshot_parse_hex(&cursor, &version);
  if (result != 0 || *cursor != '|' || version != CRT_FD_SNAPSHOT_VERSION) {
    return EINVAL;
  }
  ++cursor;
  snapshot->magic = CRT_FD_SNAPSHOT_MAGIC;
  snapshot->version = CRT_FD_SNAPSHOT_VERSION;
  snapshot->capacity = CRT_FD_SNAPSHOT_MAX;
  while (*cursor != 0) {
    struct crt_fd_snapshot_entry* entry;
    uintptr_t value = 0;

    if (snapshot->count == CRT_FD_SNAPSHOT_MAX) {
      memset(snapshot, 0, sizeof(*snapshot));
      return EMFILE;
    }
    entry = &snapshot->entries[snapshot->count];
    result = fd_snapshot_parse_hex(&cursor, &value);
    if (result != 0 || *cursor != ':') {
      memset(snapshot, 0, sizeof(*snapshot));
      return EINVAL;
    }
    entry->fd = (int)value;
    ++cursor;
    result = fd_snapshot_parse_hex(&cursor, &value);
    if (result != 0 || *cursor != ':') {
      memset(snapshot, 0, sizeof(*snapshot));
      return EINVAL;
    }
    entry->kind = (int)value;
    ++cursor;
    result = fd_snapshot_parse_hex(&cursor, &value);
    if (result != 0 || *cursor != ':') {
      memset(snapshot, 0, sizeof(*snapshot));
      return EINVAL;
    }
    entry->flags = (unsigned int)value;
    ++cursor;
    result = fd_snapshot_parse_hex(&cursor, &value);
    if (result != 0 || (*cursor != ':' && *cursor != ';')) {
      memset(snapshot, 0, sizeof(*snapshot));
      return EINVAL;
    }
    entry->handle = value;
    if (*cursor == ':') {
      ++cursor;
      result = fd_snapshot_parse_hex(&cursor, &value);
      if (result != 0 ||
          *cursor != ':' ||
          value > CRT_FD_SOCKET_PROTOCOL_INFO_SIZE) {
        memset(snapshot, 0, sizeof(*snapshot));
        return EINVAL;
      }
      ++cursor;
      entry->socket_protocol_info_size = (unsigned int)value;
      result = fd_snapshot_parse_bytes(
          &cursor,
          entry->socket_protocol_info,
          entry->socket_protocol_info_size);
      if (result != 0) {
        memset(snapshot, 0, sizeof(*snapshot));
        return EINVAL;
      }
    }
    if (*cursor != ';') {
      memset(snapshot, 0, sizeof(*snapshot));
      return EINVAL;
    }
    ++cursor;
    ++snapshot->count;
  }
  return 0;
}

#if !defined(CRT_TARGET_OS_WINDOWS)
int __crt_fd_snapshot_export(struct crt_fd_snapshot* snapshot) {
  if (snapshot == 0) {
    return EINVAL;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->magic = CRT_FD_SNAPSHOT_MAGIC;
  snapshot->version = CRT_FD_SNAPSHOT_VERSION;
  snapshot->capacity = CRT_FD_SNAPSHOT_MAX;
  return ENOTSUP;
}

int __crt_fd_snapshot_import(const struct crt_fd_snapshot* snapshot) {
  if (snapshot == 0 ||
      snapshot->magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot->version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot->count > snapshot->capacity ||
      snapshot->capacity > CRT_FD_SNAPSHOT_MAX) {
    return EINVAL;
  }
  return ENOTSUP;
}

void __crt_fd_snapshot_dispose(struct crt_fd_snapshot* snapshot) {
  if (snapshot != 0) {
    memset(snapshot, 0, sizeof(*snapshot));
  }
}

int __crt_fd_get_cloexec(int fd) {
  long result;

  result = __crt_sys_fcntl(fd, F_GETFD, 0);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return 0;
  }
  return ((int)result & FD_CLOEXEC) != 0;
}

int __crt_fd_set_cloexec(int fd, int cloexec) {
  long result;
  int flags;

  result = __crt_sys_fcntl(fd, F_GETFD, 0);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  flags = (int)result;
  if (cloexec) {
    flags |= FD_CLOEXEC;
  } else {
    flags &= ~FD_CLOEXEC;
  }
  result = __crt_sys_fcntl(fd, F_SETFD, (void*)(long)flags);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return 0;
}

void __crt_fd_after_fork_child(void) {
}
#endif

static char mkstemp_char(unsigned long value) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  return alphabet[value % (sizeof(alphabet) - 1)];
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

#if defined(CRT_TARGET_OS_WINDOWS)
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
#endif

#if !defined(CRT_TARGET_OS_WINDOWS)
static int path_append(char* path, size_t size, const char* component, size_t component_len) {
  size_t len = strlen(path);

  if (component_len == 0) {
    return 0;
  }
  if (len > 1 && !path_separator(path[len - 1])) {
    if (len + 1 >= size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    path[len++] = '/';
  }
  if (len == 0) {
    if (len + 1 >= size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    path[len++] = '/';
  }
  if (len + component_len >= size) {
    return (int)__set_errno(ENAMETOOLONG);
  }
  memcpy(path + len, component, component_len);
  path[len + component_len] = 0;
  return 0;
}

static void path_pop_component(char* path) {
  size_t len = strlen(path);

  while (len > 1 && path_separator(path[len - 1])) {
    path[--len] = 0;
  }
  while (len > 1 && !path_separator(path[len - 1])) {
    path[--len] = 0;
  }
  while (len > 1 && path_separator(path[len - 1])) {
    path[--len] = 0;
  }
  if (len == 0) {
    path[0] = '/';
    path[1] = 0;
  }
}
#endif

static int make_absolute_input(const char* path, char* absolute, size_t size) {
  char cwd[PATH_MAX];
  size_t cwd_len;
  size_t path_len;

  if (path_is_absolute(path)) {
    if (strlen(path) + 1 > size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    memcpy(absolute, path, strlen(path) + 1);
    return 0;
  }
  if (getcwd(cwd, sizeof(cwd)) == 0) {
    return -1;
  }
  cwd_len = strlen(cwd);
  path_len = strlen(path);
  if (cwd_len + 1 + path_len + 1 > size) {
    return (int)__set_errno(ENAMETOOLONG);
  }
  memcpy(absolute, cwd, cwd_len);
  absolute[cwd_len] = '/';
  memcpy(absolute + cwd_len + 1, path, path_len + 1);
  return 0;
}

#if !defined(CRT_TARGET_OS_WINDOWS)
static int resolve_symlink_path(
    const char* base,
    const char* target,
    const char* remaining,
    char* output,
    size_t size) {
  size_t len = 0;
  size_t target_len = strlen(target);
  size_t remaining_len = strlen(remaining);

  if (path_is_absolute(target)) {
    if (target_len + 1 > size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    memcpy(output, target, target_len + 1);
    len = target_len;
  } else {
    size_t base_len = strlen(base);

    if (base_len + 1 + target_len + 1 > size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    memcpy(output, base, base_len);
    len = base_len;
    if (len > 1 && !path_separator(output[len - 1])) {
      output[len++] = '/';
    }
    memcpy(output + len, target, target_len + 1);
    len += target_len;
  }
  if (remaining_len != 0) {
    if (len + 1 + remaining_len + 1 > size) {
      return (int)__set_errno(ENAMETOOLONG);
    }
    if (len == 0 || !path_separator(output[len - 1])) {
      output[len++] = '/';
    }
    memcpy(output + len, remaining, remaining_len + 1);
  }
  return 0;
}

static char* realpath_resolve_unix(const char* path, char* output, int output_owned) {
  char input[PATH_MAX];
  char resolved[PATH_MAX];
  unsigned int symlink_count = 0;

  if (make_absolute_input(path, input, sizeof(input)) != 0) {
    if (output_owned) {
      free(output);
    }
    return 0;
  }

restart:
  if (++symlink_count > 41) {
    if (output_owned) {
      free(output);
    }
    __set_errno(ELOOP);
    return 0;
  }
  resolved[0] = '/';
  resolved[1] = 0;
  {
    size_t pos = path_root_length(input);

    while (path_separator(input[pos])) {
      ++pos;
    }
    while (input[pos] != 0) {
      size_t component_start = pos;
      size_t component_len;
      char candidate[PATH_MAX];
      struct stat st;

      while (input[pos] != 0 && !path_separator(input[pos])) {
        ++pos;
      }
      component_len = pos - component_start;
      while (path_separator(input[pos])) {
        ++pos;
      }
      if (component_len == 0 ||
          (component_len == 1 && input[component_start] == '.')) {
        continue;
      }
      if (component_len == 2 && input[component_start] == '.' &&
          input[component_start + 1] == '.') {
        path_pop_component(resolved);
        continue;
      }
      memcpy(candidate, resolved, strlen(resolved) + 1);
      if (path_append(candidate, sizeof(candidate), input + component_start, component_len) != 0) {
        if (output_owned) {
          free(output);
        }
        return 0;
      }
      if (lstat(candidate, &st) != 0) {
        if (output_owned) {
          free(output);
        }
        return 0;
      }
      if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t link_len = readlink(candidate, target, sizeof(target) - 1);

        if (link_len < 0) {
          if (output_owned) {
            free(output);
          }
          return 0;
        }
        target[link_len] = 0;
        if (resolve_symlink_path(resolved, target, input + pos, input, sizeof(input)) != 0) {
          if (output_owned) {
            free(output);
          }
          return 0;
        }
        goto restart;
      }
      memcpy(resolved, candidate, strlen(candidate) + 1);
    }
  }
  if (stat(resolved, &(struct stat){0}) != 0) {
    if (output_owned) {
      free(output);
    }
    return 0;
  }
  if (strlen(resolved) + 1 > PATH_MAX) {
    if (output_owned) {
      free(output);
    }
    __set_errno(ENAMETOOLONG);
    return 0;
  }
  memcpy(output, resolved, strlen(resolved) + 1);
  return output;
}
#endif

ssize_t read(int fd, void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_read(fd, buf, (unsigned long)count));
}

ssize_t write(int fd, const void* buf, size_t count) {
  return (ssize_t)normalize_syscall_result(__crt_sys_write(fd, buf, (unsigned long)count));
}

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
  if (offset < 0) {
    return (ssize_t)__set_errno(EINVAL);
  }
  return (ssize_t)normalize_syscall_result(
      __crt_sys_pread(fd, buf, (unsigned long)count, (long long)offset));
}

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
  if (offset < 0) {
    return (ssize_t)__set_errno(EINVAL);
  }
  return (ssize_t)normalize_syscall_result(
      __crt_sys_pwrite(fd, buf, (unsigned long)count, (long long)offset));
}

int open(const char* path, int flags, ...) {
  unsigned int mode = 0;
  int syscall_flags = flags & ~O_CLOEXEC;
  va_list args;
  int fd;

  if ((flags & O_CREAT) != 0) {
    va_start(args, flags);
    mode = (unsigned int)va_arg(args, int);
    va_end(args);
  }

  if ((flags & O_DIRECTORY) != 0) {
    syscall_flags &= ~O_DIRECTORY;
  }
  fd = (int)normalize_syscall_result(__crt_sys_open(path, syscall_flags, mode));
  if (fd < 0) {
    return -1;
  }
  if ((flags & O_DIRECTORY) != 0) {
    struct stat st;

    if (fstat(fd, &st) != 0) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
      close(fd);
      return (int)__set_errno(ENOTDIR);
    }
  }
  return fd;
}

int creat(const char* path, mode_t mode) {
  return open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

static char* fill_temp_template(char* template_path, int create_file, int* fd_out) {
  static unsigned long counter;
  size_t length;
  size_t suffix;
  size_t x_count = 0;
  unsigned long attempt;

  if (fd_out != 0) {
    *fd_out = -1;
  }
  if (template_path == 0) {
    errno = EINVAL;
    return 0;
  }
  length = strlen(template_path);
  suffix = length;
  while (suffix > 0 && template_path[suffix - 1] == 'X') {
    --suffix;
    ++x_count;
  }
  if (x_count < 6) {
    errno = EINVAL;
    return 0;
  }

  for (attempt = 0; attempt < TMP_MAX; ++attempt) {
    unsigned long value = counter++ ^ (unsigned long)getpid() ^ (attempt << 16);
    size_t i;

    for (i = 0; i < x_count; ++i) {
      template_path[suffix + i] = mkstemp_char(value);
      value = value / 62UL + 0x9e3779b9UL;
    }
    if (create_file) {
      int fd = open(template_path, O_CREAT | O_EXCL | O_RDWR, 0600);
      if (fd >= 0) {
        if (fd_out != 0) {
          *fd_out = fd;
        }
        return template_path;
      }
      if (errno != EEXIST) {
        return 0;
      }
    } else if (access(template_path, F_OK) != 0) {
      if (errno == ENOENT) {
        return template_path;
      }
      return 0;
    }
  }

  errno = EEXIST;
  return 0;
}

char* mktemp(char* template_path) {
  char* result = fill_temp_template(template_path, 0, 0);

  if (result == 0 && template_path != 0) {
    template_path[0] = 0;
  }
  return result;
}

int mkstemp(char* template_path) {
  int fd = -1;

  if (fill_temp_template(template_path, 1, &fd) == 0) {
    return -1;
  }
  return fd;
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

int unlink(const char* path) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_unlink(path));
}

int ftruncate(int fd, off_t length) {
  if (length < 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_ftruncate(fd, (long long)length));
}

int fsync(int fd) {
  return (int)normalize_syscall_result(__crt_sys_fsync(fd));
}

int fdatasync(int fd) {
  return fsync(fd);
}

int truncate(const char* path, off_t length) {
  int fd;
  int result;

  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  if (length < 0) {
    return (int)__set_errno(EINVAL);
  }
  fd = open(path, O_WRONLY);
  if (fd < 0) {
    return -1;
  }
  result = ftruncate(fd, length);
  if (close(fd) != 0 && result == 0) {
    return -1;
  }
  return result;
}

int chmod(const char* path, mode_t mode) {
  if (path == 0) {
    return (int)__set_errno(EINVAL);
  }
  return (int)normalize_syscall_result(__crt_sys_chmod(path, (unsigned int)mode));
}

int fchmod(int fd, mode_t mode) {
  return (int)normalize_syscall_result(__crt_sys_fchmod(fd, (unsigned int)mode));
}

mode_t umask(mode_t mask) {
#if defined(CRT_TARGET_OS_WINDOWS)
  static mode_t current_umask;
  mode_t previous = current_umask;

  current_umask = mask & 0777;
  return previous;
#else
  long result = __crt_sys_umask((unsigned int)mask);

  if (result < 0 && result >= -4095) {
    return (mode_t)__set_errno((int)-result);
  }
  return (mode_t)result;
#endif
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
  char* output = resolved_path;
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

  if (path_is_absolute(path)) {
    if (normalize_absolute_path(path, output, PATH_MAX) != 0) {
      if (resolved_path == 0) {
        free(output);
      }
      return 0;
    }
    return output;
  }

  {
    char absolute[PATH_MAX];

    if (make_absolute_input(path, absolute, sizeof(absolute)) != 0 ||
        normalize_absolute_path(absolute, output, PATH_MAX) != 0) {
      if (resolved_path == 0) {
        free(output);
      }
      return 0;
    }
  }
  return output;
#else
  return realpath_resolve_unix(path, output, resolved_path == 0);
#endif
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

uid_t geteuid(void) {
  long result = __crt_sys_geteuid();

  if (result < 0 && result >= -4095) {
    return (uid_t)__set_errno((int)-result);
  }
  return (uid_t)result;
}

int fchown(int fd, uid_t owner, gid_t group) {
  return (int)normalize_syscall_result(
      __crt_sys_fchown(fd, (unsigned int)owner, (unsigned int)group));
}

int isatty(int fd) {
#if defined(CRT_TARGET_OS_WINDOWS)
  long result = __crt_sys_isatty(fd);

  if (result < 0 && result >= -4095) {
    __set_errno((int)-result);
    return 0;
  }
  return result != 0;
#else
  struct stat st;

  if (fstat(fd, &st) != 0) {
    return 0;
  }
  return S_ISCHR(st.st_mode);
#endif
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
    case F_DUPFD_CLOEXEC:
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
          if (cmd == F_DUPFD_CLOEXEC && __crt_fd_set_cloexec(result, 1) != 0) {
            int saved_errno = errno;

            close(result);
            while (saved_count > 0) {
              close(saved[--saved_count]);
            }
            errno = saved_errno;
            return -1;
          }
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
      return __crt_fd_get_cloexec(fd) ? FD_CLOEXEC : 0;

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
      if (__crt_fd_set_cloexec(fd, (arg & FD_CLOEXEC) != 0) != 0) {
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

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW: {
      struct flock* lock;

      va_start(args, cmd);
      lock = va_arg(args, struct flock*);
      va_end(args);
      if (lock == 0) {
        return (int)__set_errno(EINVAL);
      }
#if defined(CRT_TARGET_OS_MACOS)
      return (int)normalize_syscall_result(macos_fcntl_lock(fd, cmd, lock));
#else
      return (int)normalize_syscall_result(__crt_sys_fcntl(fd, cmd, lock));
#endif
    }

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
  st->st_atim.tv_sec = (time_t)ds->atime.tv_sec;
  st->st_atim.tv_nsec = ds->atime.tv_nsec;
  st->st_mtim.tv_sec = (time_t)ds->mtime.tv_sec;
  st->st_mtim.tv_nsec = ds->mtime.tv_nsec;
  st->st_ctim.tv_sec = (time_t)ds->ctime.tv_sec;
  st->st_ctim.tv_nsec = ds->ctime.tv_nsec;
  return 0;
}

static int macos_stat64_result(long result, const struct crt_darwin_stat64* ds, struct stat* st) {
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  return darwin_stat_to_stat(ds, st);
}

static void darwin_statfs_to_statfs(const struct crt_darwin_statfs64* ds, struct statfs* st) {
  memset(st, 0, sizeof(*st));
  st->f_type = ds->f_type;
  st->f_bsize = ds->f_bsize;
  st->f_blocks = ds->f_blocks;
  st->f_bfree = ds->f_bfree;
  st->f_bavail = ds->f_bavail;
  st->f_files = ds->f_files;
  st->f_ffree = ds->f_ffree;
  st->f_fsid = ds->f_fsid;
  st->f_namelen = 255;
  st->f_frsize = ds->f_bsize;
  st->f_flags = ds->f_flags;
}

static int macos_statfs64_result(
    long result,
    const struct crt_darwin_statfs64* ds,
    struct statfs* st) {
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  darwin_statfs_to_statfs(ds, st);
  return 0;
}

static int bionic_lock_type_to_darwin(short type) {
  switch (type) {
    case F_RDLCK:
      return CRT_DARWIN_F_RDLCK;
    case F_WRLCK:
      return CRT_DARWIN_F_WRLCK;
    case F_UNLCK:
      return CRT_DARWIN_F_UNLCK;
    default:
      return -1;
  }
}

static int darwin_lock_type_to_bionic(short type) {
  switch (type) {
    case CRT_DARWIN_F_RDLCK:
      return F_RDLCK;
    case CRT_DARWIN_F_WRLCK:
      return F_WRLCK;
    case CRT_DARWIN_F_UNLCK:
      return F_UNLCK;
    default:
      return type;
  }
}

static int bionic_fcntl_cmd_to_darwin(int cmd) {
  switch (cmd) {
    case F_GETLK:
      return CRT_DARWIN_F_GETLK;
    case F_SETLK:
      return CRT_DARWIN_F_SETLK;
    case F_SETLKW:
      return CRT_DARWIN_F_SETLKW;
    default:
      return cmd;
  }
}

static long macos_fcntl_lock(int fd, int cmd, struct flock* lock) {
  struct crt_darwin_flock darwin_lock;
  int darwin_type = bionic_lock_type_to_darwin(lock->l_type);
  long result;

  if (darwin_type < 0) {
    return -EINVAL;
  }
  memset(&darwin_lock, 0, sizeof(darwin_lock));
  darwin_lock.l_start = lock->l_start;
  darwin_lock.l_len = lock->l_len;
  darwin_lock.l_pid = lock->l_pid;
  darwin_lock.l_type = (int16_t)darwin_type;
  darwin_lock.l_whence = lock->l_whence;
  result = __crt_sys_macos_fcntl(fd, bionic_fcntl_cmd_to_darwin(cmd), &darwin_lock);
  if (result >= 0 && cmd == F_GETLK) {
    lock->l_start = darwin_lock.l_start;
    lock->l_len = darwin_lock.l_len;
    lock->l_pid = darwin_lock.l_pid;
    lock->l_type = (short)darwin_lock_type_to_bionic(darwin_lock.l_type);
    lock->l_whence = darwin_lock.l_whence;
  }
  return result;
}

static unsigned long macos_ioctl_request(unsigned long request) {
  switch (request) {
    case FIONREAD:
      return CRT_DARWIN_FIONREAD;
    case TIOCGPGRP:
      return CRT_DARWIN_TIOCGPGRP;
    case TIOCSPGRP:
      return CRT_DARWIN_TIOCSPGRP;
    case TIOCGWINSZ:
      return CRT_DARWIN_TIOCGWINSZ;
    case TIOCSWINSZ:
      return CRT_DARWIN_TIOCSWINSZ;
    default:
      return request;
  }
}

static long macos_ioctl(int fd, unsigned long request, void* arg) {
  return __crt_sys_ioctl(fd, macos_ioctl_request(request), arg);
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
    st->st_atim.tv_sec = (time_t)sx->atime.tv_sec;
    st->st_atim.tv_nsec = sx->atime.tv_nsec;
  }
  if ((sx->mask & CRT_STATX_ATTR_MTIME) != 0) {
    st->st_mtime = (time_t)sx->mtime.tv_sec;
    st->st_mtim.tv_sec = (time_t)sx->mtime.tv_sec;
    st->st_mtim.tv_nsec = sx->mtime.tv_nsec;
  }
  if ((sx->mask & CRT_STATX_ATTR_CTIME) != 0) {
    st->st_ctime = (time_t)sx->ctime.tv_sec;
    st->st_ctim.tv_sec = (time_t)sx->ctime.tv_sec;
    st->st_ctim.tv_nsec = sx->ctime.tv_nsec;
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

static int linux_fstat_procfs_fallback(int fd, struct stat* st) {
  char path[32] = "/proc/self/fd/";
  char digits[16];
  unsigned int value;
  size_t prefix_len = strlen(path);
  size_t digit_count = 0;

  if (fd < 0) {
    return (int)__set_errno(EBADF);
  }
  value = (unsigned int)fd;
  do {
    digits[digit_count++] = (char)('0' + value % 10U);
    value /= 10U;
  } while (value != 0 && digit_count < sizeof(digits));
  if (prefix_len + digit_count + 1 > sizeof(path)) {
    return (int)__set_errno(ENAMETOOLONG);
  }
  while (digit_count > 0) {
    path[prefix_len++] = digits[--digit_count];
  }
  path[prefix_len] = 0;
  return linux_statx(CRT_AT_FDCWD, path, 0, st);
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
  int result = linux_statx((long)fd, "", CRT_AT_EMPTY_PATH, st);

  if (result == 0) {
    return 0;
  }
  return linux_fstat_procfs_fallback(fd, st);
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

int statfs(const char* path, struct statfs* buf) {
  if (path == 0 || buf == 0) {
    return (int)__set_errno(EINVAL);
  }
#if defined(CRT_TARGET_OS_MACOS)
  {
    struct crt_darwin_statfs64 ds;
    memset(&ds, 0, sizeof(ds));
    return macos_statfs64_result(__crt_sys_macos_statfs64(path, &ds), &ds, buf);
  }
#else
  return (int)normalize_syscall_result(__crt_sys_statfs(path, buf));
#endif
}

int fstatfs(int fd, struct statfs* buf) {
  if (buf == 0) {
    return (int)__set_errno(EINVAL);
  }
#if defined(CRT_TARGET_OS_MACOS)
  {
    struct crt_darwin_statfs64 ds;
    memset(&ds, 0, sizeof(ds));
    return macos_statfs64_result(__crt_sys_macos_fstatfs64(fd, &ds), &ds, buf);
  }
#else
  return (int)normalize_syscall_result(__crt_sys_fstatfs(fd, buf));
#endif
}

int statfs64(const char* path, struct statfs64* buf) {
  return statfs(path, (struct statfs*)buf);
}

int fstatfs64(int fd, struct statfs64* buf) {
  return fstatfs(fd, (struct statfs*)buf);
}

int ioctl(int fd, int request, ...) {
  void* arg;
  va_list ap;

  va_start(ap, request);
  arg = va_arg(ap, void*);
  va_end(ap);
#if defined(CRT_TARGET_OS_MACOS)
  return (int)normalize_syscall_result(macos_ioctl(fd, (unsigned long)request, arg));
#else
  return (int)normalize_syscall_result(__crt_sys_ioctl(fd, (unsigned long)request, arg));
#endif
}

int mount(const char* source, const char* target, const char* fs_type, unsigned long flags,
          const void* data) {
  (void)source;
  (void)target;
  (void)fs_type;
  (void)flags;
  (void)data;
  return (int)__set_errno(ENOTSUP);
}

int umount(const char* target) {
  (void)target;
  return (int)__set_errno(ENOTSUP);
}

int umount2(const char* target, int flags) {
  (void)target;
  (void)flags;
  return (int)__set_errno(ENOTSUP);
}
