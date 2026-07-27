#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
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
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_RELEASE 0x00008000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define WINDOWS_TICK 10000000ULL
#define SEC_TO_UNIX_EPOCH 11644473600ULL

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
__declspec(dllimport) void* CRT_WINAPI VirtualAlloc(
    void* lpAddress,
    size_t dwSize,
    DWORD flAllocationType,
    DWORD flProtect);
__declspec(dllimport) BOOL CRT_WINAPI VirtualFree(
    void* lpAddress,
    size_t dwSize,
    DWORD dwFreeType);
struct crt_filetime {
  DWORD low;
  DWORD high;
};
__declspec(dllimport) void CRT_WINAPI GetSystemTimeAsFileTime(struct crt_filetime* lpSystemTimeAsFileTime);
__declspec(dllimport) BOOL CRT_WINAPI QueryPerformanceCounter(long long* lpPerformanceCount);
__declspec(dllimport) BOOL CRT_WINAPI QueryPerformanceFrequency(long long* lpFrequency);
__declspec(dllimport) void CRT_WINAPI Sleep(DWORD dwMilliseconds);
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

void* __crt_sys_mmap(void* addr, unsigned long length, int prot, int flags, int fd, long long offset) {
  DWORD protect;
  void* result;
  (void)offset;

  if ((flags & MAP_ANONYMOUS) == 0 || fd != -1) {
    return (void*)(intptr_t)-ENOSYS;
  }

  if ((prot & PROT_WRITE) != 0 && (prot & PROT_EXEC) != 0) {
    protect = PAGE_EXECUTE_READWRITE;
  } else if ((prot & PROT_WRITE) != 0) {
    protect = PAGE_READWRITE;
  } else if ((prot & PROT_EXEC) != 0 && (prot & PROT_READ) != 0) {
    protect = PAGE_EXECUTE_READ;
  } else if ((prot & PROT_EXEC) != 0) {
    protect = PAGE_EXECUTE;
  } else if ((prot & PROT_READ) != 0) {
    protect = PAGE_READONLY;
  } else {
    protect = PAGE_NOACCESS;
  }

  result = VirtualAlloc(addr, (size_t)length, MEM_RESERVE | MEM_COMMIT, protect);
  if (result == 0) {
    return (void*)(intptr_t)-map_windows_error(GetLastError());
  }
  return result;
}

long __crt_sys_munmap(void* addr, unsigned long length) {
  (void)length;
  if (!VirtualFree(addr, 0, MEM_RELEASE)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_gettimeofday(struct timeval* tv) {
  struct crt_filetime ft;
  unsigned long long ticks;

  GetSystemTimeAsFileTime(&ft);
  ticks = ((unsigned long long)ft.high << 32) | ft.low;
  ticks -= SEC_TO_UNIX_EPOCH * WINDOWS_TICK;
  tv->tv_sec = (time_t)(ticks / WINDOWS_TICK);
  tv->tv_usec = (long)((ticks % WINDOWS_TICK) / 10ULL);
  return 0;
}

long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (clock_id == CLOCK_REALTIME) {
    struct timeval tv;
    long result = __crt_sys_gettimeofday(&tv);
    if (result != 0) {
      return result;
    }
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000L;
    return 0;
  }

  if (clock_id == CLOCK_MONOTONIC) {
    long long count;
    long long frequency;
    long long seconds;
    long long remainder;

    if (!QueryPerformanceFrequency(&frequency) || frequency <= 0 ||
        !QueryPerformanceCounter(&count)) {
      return fail_last_error();
    }
    seconds = count / frequency;
    remainder = count % frequency;
    tp->tv_sec = (time_t)seconds;
    tp->tv_nsec = (long)((remainder * 1000000000LL) / frequency);
    return 0;
  }

  return -EINVAL;
}

long __crt_sys_nanosleep(const struct timespec* req, struct timespec* rem) {
  unsigned long long ms;
  (void)rem;

  if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) {
    return -EINVAL;
  }
  ms = (unsigned long long)req->tv_sec * 1000ULL + ((unsigned long long)req->tv_nsec + 999999ULL) / 1000000ULL;
  if (ms > 0xffffffffULL) {
    ms = 0xffffffffULL;
  }
  Sleep((DWORD)ms);
  return 0;
}

long __crt_sys_sched_yield(void) {
  Sleep(0);
  return 0;
}

void __crt_sys_exit(int status) {
  ExitProcess((unsigned int)status);
}
