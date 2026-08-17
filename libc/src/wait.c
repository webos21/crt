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

/*
 * Non-private futex operations: same op codes without the _PRIVATE bit.
 * These key off the futex's physical backing (the mapped page + offset)
 * instead of (mm_struct, virtual address), which is what makes them
 * correctly coordinate waiters across independent processes sharing the
 * same PTHREAD_PROCESS_SHARED memory -- the private operations above do
 * NOT do this correctly cross-process even over genuinely shared memory.
 */
#define CRT_FUTEX_WAIT 0
#define CRT_FUTEX_WAKE 1

int __crt_wait32_shared(int* addr, int expected) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAIT, expected, 0, 0, 0);
  if (result == 0 || result == -EAGAIN || result == -EINTR) {
    return 0;
  }
  return result < 0 ? (int)-result : 0;
}

int __crt_wait32_timed_shared(int* addr, int expected, const struct timespec* timeout) {
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
  result = __crt_sys_futex(addr, CRT_FUTEX_WAIT, expected, timeout, 0, 0);
  if (result == 0 || result == -EAGAIN || result == -EINTR) {
    return 0;
  }
  return result < 0 ? (int)-result : 0;
}

int __crt_wake32_one_shared(int* addr) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAKE, 1, 0, 0, 0);
  return result < 0 ? (int)-result : 0;
}

int __crt_wake32_all_shared(int* addr) {
  long result;

  if (addr == 0) {
    return EINVAL;
  }
  result = __crt_sys_futex(addr, CRT_FUTEX_WAKE, CRT_INT_MAX, 0, 0, 0);
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

/*
 * WaitOnAddress/WakeByAddressSingle/WakeByAddressAll are documented by
 * Microsoft to operate on addresses within the calling process's own
 * virtual address space -- there is no flag or variant that extends them
 * across process boundaries. This is an architectural limitation of the
 * API, not a missing feature this code could opt into, so the honest
 * answer here is ENOTSUP rather than a fabricated cross-process wait. A
 * real fix would need an entirely different mechanism (e.g. a named
 * kernel object such as CreateMutexA/CreateEventA with a shared name, or
 * handle duplication/inheritance) -- out of scope for this primitive.
 */
int __crt_wait32_shared(int* addr, int expected) {
  (void)addr;
  (void)expected;
  return ENOTSUP;
}

int __crt_wait32_timed_shared(int* addr, int expected, const struct timespec* timeout) {
  (void)addr;
  (void)expected;
  (void)timeout;
  return ENOTSUP;
}

int __crt_wake32_one_shared(int* addr) {
  (void)addr;
  return ENOTSUP;
}

int __crt_wake32_all_shared(int* addr) {
  (void)addr;
  return ENOTSUP;
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

/*
 * os_sync_wait_on_address()/os_sync_wake_by_address_*() (libSystem,
 * <os/os_sync_wait_on_address.h>, macOS 14.4+/iOS 17.4+) document a
 * OS_SYNC_WAIT_ON_ADDRESS_SHARED / OS_SYNC_WAKE_BY_ADDRESS_SHARED flag bit
 * (value 0x1) that opts an address into cross-process waiting, mirroring
 * this file's flags==0 (private/process-local) default used above. This
 * mirrors the Linux private/shared split. UNVERIFIED: this dev session is
 * Windows-only (see docs/bionic_libc_gaps.md and HISTORY.md's linkat()
 * precedent for the same discipline) -- the flag's existence and value are
 * reasoned from the documented header shape, not confirmed on real macOS
 * hardware. Flag as unverified until confirmed by real macOS CI/hardware.
 */
#define CRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED 0x00000001U
#define CRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED 0x00000001U

int __crt_wait32_shared(int* addr, int expected) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected) {
    return 0;
  }
  result = os_sync_wait_on_address(
      addr, (uint32_t)expected, sizeof(*addr), CRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED);
  return map_macos_wait_result(result);
}

int __crt_wait32_timed_shared(int* addr, int expected, const struct timespec* timeout) {
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
      CRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED,
      CRT_OS_CLOCK_MACH_ABSOLUTE_TIME,
      timeout_to_nanoseconds(timeout));
  return map_macos_wait_result(result);
}

int __crt_wake32_one_shared(int* addr) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  result = os_sync_wake_by_address_any(addr, sizeof(*addr), CRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED);
  return map_macos_wait_result(result);
}

int __crt_wake32_all_shared(int* addr) {
  int result;

  if (addr == 0) {
    return EINVAL;
  }
  result = os_sync_wake_by_address_all(addr, sizeof(*addr), CRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED);
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

/*
 * This fallback path has no real blocking primitive at all (it busy-polls
 * via sched_yield()), so it never distinguished private from shared
 * addresses to begin with -- the shared variants are plain aliases.
 */
int __crt_wait32_shared(int* addr, int expected) {
  return __crt_wait32(addr, expected);
}

int __crt_wait32_timed_shared(int* addr, int expected, const struct timespec* timeout) {
  return __crt_wait32_timed(addr, expected, timeout);
}

int __crt_wake32_one_shared(int* addr) {
  return __crt_wake32_one(addr);
}

int __crt_wake32_all_shared(int* addr) {
  return __crt_wake32_all(addr);
}
#endif
