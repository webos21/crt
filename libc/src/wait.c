#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>

#include <private/crt_wait.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_futex(int* addr, int op, int value, const void* timeout, int* addr2, int value3);

#define CRT_FUTEX_WAIT_PRIVATE 128
#define CRT_FUTEX_WAKE_PRIVATE 129
#define CRT_INT_MAX 2147483647

int __crt_wait32(int* addr, int expected) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAIT_PRIVATE, expected, 0, 0, 0);
  if (result == 0 || result == -EAGAIN || result == -EINTR) {
    return 0;
  }
  return result < 0 ? (int)-result : 0;
}

int __crt_wake32_one(int* addr) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
  return result < 0 ? (int)-result : 0;
}

int __crt_wake32_all(int* addr) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAKE_PRIVATE, CRT_INT_MAX, 0, 0, 0);
  return result < 0 ? (int)-result : 0;
}

#elif defined(CRT_TARGET_OS_WINDOWS)
typedef unsigned long DWORD;
typedef int BOOL;

#define CRT_WINAPI
#define CRT_INFINITE 0xffffffffUL

__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);
__declspec(dllimport) BOOL CRT_WINAPI WaitOnAddress(
    volatile void* Address,
    void* CompareAddress,
    size_t AddressSize,
    DWORD dwMilliseconds);
__declspec(dllimport) void CRT_WINAPI WakeByAddressSingle(void* Address);
__declspec(dllimport) void CRT_WINAPI WakeByAddressAll(void* Address);

static int map_windows_wait_error(DWORD error) {
  switch (error) {
    case 0:
      return 0;
    case 5:
      return EACCES;
    case 87:
      return EINVAL;
    default:
      return EIO;
  }
}

int __crt_wait32(int* addr, int expected) {
  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  if (!WaitOnAddress(addr, &expected, sizeof(expected), CRT_INFINITE)) {
    return map_windows_wait_error(GetLastError());
  }
  return 0;
}

int __crt_wake32_one(int* addr) {
  if (addr == 0) {
    return EINVAL;
  }
  WakeByAddressSingle(addr);
  return 0;
}

int __crt_wake32_all(int* addr) {
  if (addr == 0) {
    return EINVAL;
  }
  WakeByAddressAll(addr);
  return 0;
}

#elif defined(CRT_TARGET_OS_MACOS)
typedef int (*crt_os_sync_wait_on_address_fn)(void*, uint64_t, size_t, uint32_t);
typedef int (*crt_os_sync_wake_by_address_fn)(void*, size_t, uint32_t);

#define CRT_RTLD_NEXT ((void*)-1)
#define CRT_OS_SYNC_WAIT_ON_ADDRESS_NONE 0U
#define CRT_OS_SYNC_WAKE_BY_ADDRESS_NONE 0U

void* dlsym(void* handle, const char* symbol);

static crt_os_sync_wait_on_address_fn macos_wait_fn(void) {
  return (crt_os_sync_wait_on_address_fn)dlsym(CRT_RTLD_NEXT, "os_sync_wait_on_address");
}

static crt_os_sync_wake_by_address_fn macos_wake_one_fn(void) {
  return (crt_os_sync_wake_by_address_fn)dlsym(CRT_RTLD_NEXT, "os_sync_wake_by_address_any");
}

static crt_os_sync_wake_by_address_fn macos_wake_all_fn(void) {
  return (crt_os_sync_wake_by_address_fn)dlsym(CRT_RTLD_NEXT, "os_sync_wake_by_address_all");
}

int __crt_wait32(int* addr, int expected) {
  crt_os_sync_wait_on_address_fn wait_fn;

  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }

  wait_fn = macos_wait_fn();
  if (wait_fn == 0) {
    sched_yield();
    return 0;
  }
  if (wait_fn(addr, (uint32_t)expected, sizeof(expected), CRT_OS_SYNC_WAIT_ON_ADDRESS_NONE) < 0) {
    return errno;
  }
  return 0;
}

int __crt_wake32_one(int* addr) {
  crt_os_sync_wake_by_address_fn wake_fn;

  if (addr == 0) {
    return EINVAL;
  }
  wake_fn = macos_wake_one_fn();
  if (wake_fn == 0) {
    return 0;
  }
  if (wake_fn(addr, sizeof(int), CRT_OS_SYNC_WAKE_BY_ADDRESS_NONE) < 0) {
    return errno;
  }
  return 0;
}

int __crt_wake32_all(int* addr) {
  crt_os_sync_wake_by_address_fn wake_fn;

  if (addr == 0) {
    return EINVAL;
  }
  wake_fn = macos_wake_all_fn();
  if (wake_fn == 0) {
    return 0;
  }
  if (wake_fn(addr, sizeof(int), CRT_OS_SYNC_WAKE_BY_ADDRESS_NONE) < 0) {
    return errno;
  }
  return 0;
}

#else
int __crt_wait32(int* addr, int expected) {
  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) == expected) {
    sched_yield();
  }
  return 0;
}

int __crt_wake32_one(int* addr) {
  return addr == 0 ? EINVAL : 0;
}

int __crt_wake32_all(int* addr) {
  return addr == 0 ? EINVAL : 0;
}
#endif
