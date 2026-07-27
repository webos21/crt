typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

__declspec(dllimport) HANDLE CRT_WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) BOOL CRT_WINAPI WriteFile(
    HANDLE hFile,
    const void* lpBuffer,
    DWORD nNumberOfBytesToWrite,
    DWORD* lpNumberOfBytesWritten,
    void* lpOverlapped);
__declspec(dllimport) void CRT_WINAPI ExitProcess(unsigned int uExitCode);

long __crt_sys_write(int fd, const void* buf, unsigned long count) {
  HANDLE handle;
  DWORD written = 0;

  if (fd == 1) {
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
  } else if (fd == 2) {
    handle = GetStdHandle(STD_ERROR_HANDLE);
  } else {
    return -1;
  }

  if (!WriteFile(handle, buf, (DWORD)count, &written, 0)) {
    return -1;
  }
  return (long)written;
}

void __crt_sys_exit(int status) {
  ExitProcess((unsigned int)status);
}
