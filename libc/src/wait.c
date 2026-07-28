#include <errno.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <private/crt_wait.h>

static int timeout_is_zero_or_negative(const struct timespec* timeout) {
  return timeout != 0 && (timeout->tv_sec < 0 || (timeout->tv_sec == 0 && timeout->tv_nsec <= 0));
}

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

int __crt_wait32_timed(int* addr, int expected, const struct timespec* timeout) {
  long result;

  if (addr == 0 || timeout == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  if (timeout_is_zero_or_negative(timeout)) {
    return ETIMEDOUT;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAIT_PRIVATE, expected, timeout, 0, 0);
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
#define CRT_ERROR_TIMEOUT 1460UL

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
    case CRT_ERROR_TIMEOUT:
      return ETIMEDOUT;
    default:
      return EIO;
  }
}

static DWORD timeout_to_milliseconds(const struct timespec* timeout) {
  uint64_t milliseconds;

  if (timeout->tv_sec <= 0 && timeout->tv_nsec <= 0) {
    return 0;
  }
  milliseconds = (uint64_t)timeout->tv_sec * 1000ULL;
  milliseconds += ((uint64_t)timeout->tv_nsec + 999999ULL) / 1000000ULL;
  if (milliseconds > 0xfffffffeULL) {
    return 0xfffffffeUL;
  }
  return (DWORD)milliseconds;
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

int __crt_wait32_timed(int* addr, int expected, const struct timespec* timeout) {
  if (addr == 0 || timeout == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  if (timeout_is_zero_or_negative(timeout)) {
    return ETIMEDOUT;
  }
  if (!WaitOnAddress(addr, &expected, sizeof(expected), timeout_to_milliseconds(timeout))) {
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
typedef int os_clockid_t;
typedef uint32_t os_sync_wait_on_address_flags_t;
typedef uint32_t os_sync_wake_by_address_flags_t;

#define CRT_OS_CLOCK_MACH_ABSOLUTE_TIME 32
#define CRT_DARWIN_ETIMEDOUT 60

int* __error(void);
int os_sync_wait_on_address(
    void* addr,
    uint64_t value,
    size_t size,
    os_sync_wait_on_address_flags_t flags);
int os_sync_wait_on_address_with_timeout(
    void* addr,
    uint64_t value,
    size_t size,
    os_sync_wait_on_address_flags_t flags,
    os_clockid_t clockid,
    uint64_t timeout_ns);
int os_sync_wake_by_address_any(void* addr, size_t size, os_sync_wake_by_address_flags_t flags);
int os_sync_wake_by_address_all(void* addr, size_t size, os_sync_wake_by_address_flags_t flags);

static int map_macos_wait_result(int result) {
  int error;

  if (result >= 0) {
    return 0;
  }
  error = *__error();
  if (error == CRT_DARWIN_ETIMEDOUT) {
    return ETIMEDOUT;
  }
  if (error == ENOENT || error == EFAULT || error == ENOMEM) {
    return 0;
  }
  return error;
}

static uint64_t timeout_to_nanoseconds(const struct timespec* timeout) {
  uint64_t nanoseconds = (uint64_t)timeout->tv_sec * 1000000000ULL;

  nanoseconds += (uint64_t)timeout->tv_nsec;
  return nanoseconds == 0 ? 1 : nanoseconds;
}

int __crt_wait32(int* addr, int expected) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  result = os_sync_wait_on_address(addr, (uint32_t)expected, sizeof(*addr), 0);
  return map_macos_wait_result(result);
}

int __crt_wait32_timed(int* addr, int expected, const struct timespec* timeout) {
  int result;

  if (addr == 0 || timeout == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  if (timeout_is_zero_or_negative(timeout)) {
    return ETIMEDOUT;
  }
  result = os_sync_wait_on_address_with_timeout(
      addr,
      (uint32_t)expected,
      sizeof(*addr),
      0,
      CRT_OS_CLOCK_MACH_ABSOLUTE_TIME,
      timeout_to_nanoseconds(timeout));
  return map_macos_wait_result(result);
}

int __crt_wake32_one(int* addr) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  result = os_sync_wake_by_address_any(addr, sizeof(*addr), 0);
  return map_macos_wait_result(result);
}

int __crt_wake32_all(int* addr) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  result = os_sync_wake_by_address_all(addr, sizeof(*addr), 0);
  return map_macos_wait_result(result);
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

int __crt_wait32_timed(int* addr, int expected, const struct timespec* timeout) {
  if (addr == 0 || timeout == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) == expected) {
    if (timeout_is_zero_or_negative(timeout)) {
      return ETIMEDOUT;
    }
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
