#include <errno.h>

#if defined(CRT_TARGET_OS_WINDOWS)
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#define CRT_WINAPI
#define CRT_TLS_OUT_OF_INDEXES ((DWORD)0xffffffffUL)
#define CRT_MEM_COMMIT 0x00001000
#define CRT_MEM_RESERVE 0x00002000
#define CRT_PAGE_READWRITE 0x04

__declspec(dllimport) DWORD CRT_WINAPI TlsAlloc(void);
__declspec(dllimport) BOOL CRT_WINAPI TlsFree(DWORD dwTlsIndex);
__declspec(dllimport) void* CRT_WINAPI TlsGetValue(DWORD dwTlsIndex);
__declspec(dllimport) BOOL CRT_WINAPI TlsSetValue(DWORD dwTlsIndex, void* lpTlsValue);
__declspec(dllimport) long CRT_WINAPI InterlockedCompareExchange(volatile long* Destination, long Exchange, long Comperand);
__declspec(dllimport) void* CRT_WINAPI VirtualAlloc(
    void* lpAddress,
    unsigned long long dwSize,
    DWORD flAllocationType,
    DWORD flProtect);

static volatile long __crt_errno_tls_index = (long)CRT_TLS_OUT_OF_INDEXES;
static int __crt_errno_fallback;

static int* __crt_windows_errno(void) {
  DWORD index;
  int* value;

  index = (DWORD)__crt_errno_tls_index;
  if (index == CRT_TLS_OUT_OF_INDEXES) {
    DWORD new_index = TlsAlloc();
    if (new_index == CRT_TLS_OUT_OF_INDEXES) {
      return &__crt_errno_fallback;
    }
    if (InterlockedCompareExchange(&__crt_errno_tls_index, (long)new_index,
                                   (long)CRT_TLS_OUT_OF_INDEXES) != (long)CRT_TLS_OUT_OF_INDEXES) {
      TlsFree(new_index);
      index = (DWORD)__crt_errno_tls_index;
    } else {
      index = new_index;
    }
  }

  value = (int*)TlsGetValue(index);
  if (value == 0) {
    value = (int*)VirtualAlloc(0, sizeof(int), CRT_MEM_RESERVE | CRT_MEM_COMMIT, CRT_PAGE_READWRITE);
    if (value == 0 || !TlsSetValue(index, value)) {
      return &__crt_errno_fallback;
    }
  }
  return value;
}
#else
static __thread int __crt_errno_value;
#endif

int* __errno(void) {
#if defined(CRT_TARGET_OS_WINDOWS)
  return __crt_windows_errno();
#else
  return &__crt_errno_value;
#endif
}

int __set_errno(int value) {
  errno = value;
  return -1;
}
