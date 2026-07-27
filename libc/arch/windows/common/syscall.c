#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#define CRT_FD_TABLE_SIZE 64

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define GENERIC_READ ((DWORD)0x80000000)
#define GENERIC_WRITE ((DWORD)0x40000000)
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

__declspec(dllimport) HANDLE CRT_WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);
__declspec(dllimport) HANDLE CRT_WINAPI CreateFileA(
    const char* lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    void* lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
__declspec(dllimport) BOOL CRT_WINAPI ReadFile(
    HANDLE hFile,
    void* lpBuffer,
    DWORD nNumberOfBytesToRead,
    DWORD* lpNumberOfBytesRead,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI WriteFile(
    HANDLE hFile,
    const void* lpBuffer,
    DWORD nNumberOfBytesToWrite,
    DWORD* lpNumberOfBytesWritten,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) BOOL CRT_WINAPI SetFilePointerEx(
    HANDLE hFile,
    long long liDistanceToMove,
    long long* lpNewFilePointer,
    DWORD dwMoveMethod);
__declspec(dllimport) void CRT_WINAPI ExitProcess(unsigned int uExitCode);

static HANDLE fd_table[CRT_FD_TABLE_SIZE];
static int fd_table_initialized;

static int map_windows_error(DWORD error) {
  switch (error) {
    case 2:
    case 3:
      return ENOENT;
    case 5:
      return EACCES;
    case 6:
      return EBADF;
    case 32:
      return EBUSY;
    case 80:
    case 183:
      return EEXIST;
    case 87:
      return EINVAL;
    default:
      return EIO;
  }
}

static long fail_last_error(void) {
  return -map_windows_error(GetLastError());
}

static void init_fd_table(void) {
  if (fd_table_initialized) {
    return;
  }
  fd_table[0] = GetStdHandle(STD_INPUT_HANDLE);
  fd_table[1] = GetStdHandle(STD_OUTPUT_HANDLE);
  fd_table[2] = GetStdHandle(STD_ERROR_HANDLE);
  fd_table_initialized = 1;
}

static HANDLE get_fd_handle(int fd) {
  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_table[fd] == 0 ||
      fd_table[fd] == INVALID_HANDLE_VALUE) {
    return INVALID_HANDLE_VALUE;
  }
  return fd_table[fd];
}

static int alloc_fd(HANDLE handle) {
  int fd;

  init_fd_table();
  for (fd = 3; fd < CRT_FD_TABLE_SIZE; ++fd) {
    if (fd_table[fd] == 0) {
      fd_table[fd] = handle;
      return fd;
    }
  }
  return -1;
}

long __crt_sys_read(int fd, void* buf, unsigned long count) {
  HANDLE handle = get_fd_handle(fd);
  DWORD bytes_read = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (!ReadFile(handle, buf, (DWORD)count, &bytes_read, 0)) {
    return fail_last_error();
  }
  return (long)bytes_read;
}

long __crt_sys_write(int fd, const void* buf, unsigned long count) {
  HANDLE handle = get_fd_handle(fd);
  DWORD written = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (!WriteFile(handle, buf, (DWORD)count, &written, 0)) {
    return fail_last_error();
  }
  return (long)written;
}

long __crt_sys_open(const char* path, int flags, unsigned int mode) {
  DWORD access = 0;
  DWORD disposition = OPEN_EXISTING;
  HANDLE handle;
  int fd;
  (void)mode;

  if ((flags & O_RDWR) == O_RDWR) {
    access = GENERIC_READ | GENERIC_WRITE;
  } else if (flags & O_WRONLY) {
    access = GENERIC_WRITE;
  } else {
    access = GENERIC_READ;
  }

  if ((flags & O_CREAT) && (flags & O_TRUNC)) {
    disposition = CREATE_ALWAYS;
  } else if (flags & O_CREAT) {
    disposition = OPEN_ALWAYS;
  } else {
    disposition = OPEN_EXISTING;
  }

  handle = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       0, disposition, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    return fail_last_error();
  }

  fd = alloc_fd(handle);
  if (fd < 0) {
    CloseHandle(handle);
    return -EMFILE;
  }
  return fd;
}

long __crt_sys_close(int fd) {
  HANDLE handle = get_fd_handle(fd);

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (fd >= 0 && fd <= 2) {
    fd_table[fd] = 0;
    return 0;
  }
  fd_table[fd] = 0;
  if (!CloseHandle(handle)) {
    return fail_last_error();
  }
  return 0;
}

long long __crt_sys_lseek(int fd, long long offset, int whence) {
  HANDLE handle = get_fd_handle(fd);
  DWORD method;
  long long new_position = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }

  if (whence == SEEK_SET) {
    method = FILE_BEGIN;
  } else if (whence == SEEK_CUR) {
    method = FILE_CURRENT;
  } else if (whence == SEEK_END) {
    method = FILE_END;
  } else {
    return -EINVAL;
  }

  if (!SetFilePointerEx(handle, offset, &new_position, method)) {
    return fail_last_error();
  }
  return new_position;
}

void __crt_sys_exit(int status) {
  ExitProcess((unsigned int)status);
}
