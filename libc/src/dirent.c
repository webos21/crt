#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CRT_DIRENT_BUFFER_SIZE 4096

#if defined(CRT_TARGET_OS_WINDOWS)
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#define CRT_WIN_MAX_PATH 260

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

struct crt_filetime {
  DWORD low;
  DWORD high;
};

struct crt_win_find_data {
  DWORD file_attributes;
  struct crt_filetime creation_time;
  struct crt_filetime last_access_time;
  struct crt_filetime last_write_time;
  DWORD file_size_high;
  DWORD file_size_low;
  DWORD reserved0;
  DWORD reserved1;
  char file_name[CRT_WIN_MAX_PATH];
  char alternate_file_name[14];
};

__declspec(dllimport) HANDLE CRT_WINAPI FindFirstFileA(
    const char* lpFileName,
    struct crt_win_find_data* lpFindFileData);
__declspec(dllimport) BOOL CRT_WINAPI FindNextFileA(
    HANDLE hFindFile,
    struct crt_win_find_data* lpFindFileData);
__declspec(dllimport) BOOL CRT_WINAPI FindClose(HANDLE hFindFile);

struct __crt_DIR {
  HANDLE handle;
  int first_pending;
  struct crt_win_find_data find_data;
  struct dirent entry;
};
#else
long __crt_sys_getdents(int fd, void* buf, unsigned long size);

struct __crt_DIR {
  int fd;
  char buffer[CRT_DIRENT_BUFFER_SIZE];
  size_t pos;
  size_t len;
  long basep;
  struct dirent entry;
};
#endif

static void copy_name(char* dest, const char* src) {
  size_t i = 0;

  while (i < sizeof(((struct dirent*)0)->d_name) - 1 && src[i] != 0) {
    dest[i] = src[i];
    ++i;
  }
  dest[i] = 0;
}

#if defined(CRT_TARGET_OS_WINDOWS)
static unsigned char windows_dirent_type(DWORD attrs) {
  if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    return DT_LNK;
  }
  if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return DT_DIR;
  }
  return DT_REG;
}

static int make_find_pattern(const char* path, char* pattern, size_t size) {
  size_t len;

  if (path == 0 || path[0] == 0) {
    errno = EINVAL;
    return -1;
  }
  len = strlen(path);
  if (len + 3 > size) {
    errno = ERANGE;
    return -1;
  }
  memcpy(pattern, path, len);
  if (len != 0 && pattern[len - 1] != '/' && pattern[len - 1] != '\\') {
    pattern[len++] = '\\';
  }
  pattern[len++] = '*';
  pattern[len] = 0;
  return 0;
}

DIR* opendir(const char* path) {
  char pattern[CRT_WIN_MAX_PATH + 3];
  DIR* dir;

  if (make_find_pattern(path, pattern, sizeof(pattern)) != 0) {
    return 0;
  }
  dir = (DIR*)malloc(sizeof(DIR));
  if (dir == 0) {
    errno = ENOMEM;
    return 0;
  }
  dir->handle = FindFirstFileA(pattern, &dir->find_data);
  if (dir->handle == INVALID_HANDLE_VALUE) {
    free(dir);
    errno = ENOENT;
    return 0;
  }
  dir->first_pending = 1;
  return dir;
}

struct dirent* readdir(DIR* dirp) {
  if (dirp == 0) {
    errno = EBADF;
    return 0;
  }
  if (dirp->first_pending) {
    dirp->first_pending = 0;
  } else if (!FindNextFileA(dirp->handle, &dirp->find_data)) {
    return 0;
  }
  dirp->entry.d_ino = 0;
  dirp->entry.d_type = windows_dirent_type(dirp->find_data.file_attributes);
  copy_name(dirp->entry.d_name, dirp->find_data.file_name);
  return &dirp->entry;
}

int closedir(DIR* dirp) {
  if (dirp == 0) {
    errno = EBADF;
    return -1;
  }
  if (!FindClose(dirp->handle)) {
    free(dirp);
    errno = EIO;
    return -1;
  }
  free(dirp);
  return 0;
}
#else
static unsigned char host_dirent_type(unsigned char type) {
  switch (type) {
    case DT_FIFO:
    case DT_CHR:
    case DT_DIR:
    case DT_BLK:
    case DT_REG:
    case DT_LNK:
    case DT_SOCK:
      return type;
    default:
      return DT_UNKNOWN;
  }
}

DIR* opendir(const char* path) {
  DIR* dir;
  struct stat st;
  int fd;

  if (path == 0) {
    errno = EINVAL;
    return 0;
  }
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    return 0;
  }
  if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode)) {
    close(fd);
    errno = ENOTDIR;
    return 0;
  }
  dir = (DIR*)malloc(sizeof(DIR));
  if (dir == 0) {
    close(fd);
    errno = ENOMEM;
    return 0;
  }
  dir->fd = fd;
  dir->pos = 0;
  dir->len = 0;
  dir->basep = 0;
  return dir;
}

struct dirent* readdir(DIR* dirp) {
  if (dirp == 0) {
    errno = EBADF;
    return 0;
  }
  for (;;) {
    unsigned short reclen;
    const char* name;
    unsigned char type;

    if (dirp->pos >= dirp->len) {
      long result = __crt_sys_getdents(dirp->fd, dirp->buffer, sizeof(dirp->buffer));
      if (result < 0 && result >= -4095) {
        errno = (int)-result;
        return 0;
      }
      if (result == 0) {
        return 0;
      }
      dirp->pos = 0;
      dirp->len = (size_t)result;
    }

#if defined(CRT_TARGET_OS_LINUX)
    reclen = *(unsigned short*)(void*)(dirp->buffer + dirp->pos + 16);
    type = *(unsigned char*)(void*)(dirp->buffer + dirp->pos + reclen - 1);
    name = dirp->buffer + dirp->pos + 19;
    dirp->entry.d_ino = *(uint64_t*)(void*)(dirp->buffer + dirp->pos);
#else
    reclen = *(unsigned short*)(void*)(dirp->buffer + dirp->pos + 16);
    type = *(unsigned char*)(void*)(dirp->buffer + dirp->pos + 20);
    name = dirp->buffer + dirp->pos + 21;
    dirp->entry.d_ino = *(uint64_t*)(void*)(dirp->buffer + dirp->pos);
#endif
    if (reclen == 0 || dirp->pos + reclen > dirp->len) {
      errno = EIO;
      return 0;
    }
    dirp->pos += reclen;
    dirp->entry.d_type = host_dirent_type(type);
    copy_name(dirp->entry.d_name, name);
    return &dirp->entry;
  }
}

int closedir(DIR* dirp) {
  int result;

  if (dirp == 0) {
    errno = EBADF;
    return -1;
  }
  result = close(dirp->fd);
  free(dirp);
  return result;
}
#endif
