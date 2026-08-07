/* Minimal, project-owned <windows.h> shim -- NOT a Windows SDK header.
 *
 * Exists for one narrow purpose: a handful of third-party port sources
 * (currently just libffi's src/dlmalloc.c and src/aarch64/ffi.c) do
 * `#include <windows.h>` on Windows to reach a small, fixed set of real
 * Win32 APIs (VirtualAlloc-family memory management, GetSystemInfo,
 * FlushInstructionCache, a couple of Interlocked* primitives) that this
 * project's own sysroot doesn't expose through a public header -- its
 * Win32 surface is declared privately inside libc/src/arch/windows/,
 * matching Bionic's own policy of not exposing host-OS APIs publicly.
 *
 * Deliberately NOT a general-purpose <windows.h> replacement: only the
 * exact declarations the ported sources above actually call are here,
 * in the same plain `__declspec(dllimport)` style already used
 * throughout libc/src/arch/windows/ (e.g. fork_memcopy.c, fork_capable_
 * relaunch.c, compiler_abi.c) -- verified field-for-field against a real
 * Windows SDK winnt.h/sysinfoapi.h the same way those files were.
 *
 * This is preferred over the alternative (hiding _WIN32/WIN32 from these
 * files via -U CFLAGS so they take their generic POSIX-mmap path
 * instead, which this project's own libc genuinely supports): that
 * approach was tried first for libffi and reverted after it turned out
 * _WIN32 also gates several *other*, unrelated decisions in libffi's own
 * source (aarch64/ffitarget.h disabling Go-closures because Windows
 * reserves the X18 register; FFI_DEFAULT_ABI selecting FFI_WIN64; a
 * caller compiling against the installed ffi.h normally -- without any
 * special -U flags -- would see a *different* FFI_DEFAULT_ABI than what
 * the library itself was built understanding). Keeping _WIN32 defined
 * normally and providing this header instead keeps every one of those
 * upstream, Windows-aware decisions consistent with what any real
 * consumer of the installed library will also see, at the cost of only
 * needing to satisfy the specific #include <windows.h> that started
 * this. See porting/recipes/libffi.json's own notes for the full story. */

typedef void* HANDLE;
typedef void* LPVOID;
typedef void* PVOID;
typedef const void* LPCVOID;
typedef unsigned long DWORD;
typedef unsigned long* DWORD_PTR;
typedef unsigned short WORD;
typedef int BOOL;
typedef long LONG;
typedef long volatile* LONG_PTR_VOLATILE;
typedef unsigned long long DWORDLONG;
typedef unsigned long long SIZE_T;

#define WINAPI

#define MEM_COMMIT 0x00001000UL
#define MEM_RESERVE 0x00002000UL
#define MEM_RELEASE 0x00008000UL
#define MEM_TOP_DOWN 0x00100000UL
#define PAGE_READWRITE 0x04UL
#define PAGE_EXECUTE_READWRITE 0x40UL

/* MEMORY_BASIC_INFORMATION (winnt.h): field-for-field match, including
 * the _WIN64-only PartitionId gap -- this project only targets 64-bit
 * Windows (aarch64/x86_64), so that branch always applies. */
typedef struct _MEMORY_BASIC_INFORMATION {
  LPVOID BaseAddress;
  LPVOID AllocationBase;
  DWORD AllocationProtect;
  WORD PartitionId;
  SIZE_T RegionSize;
  DWORD State;
  DWORD Protect;
  DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

/* SYSTEM_INFO (sysinfoapi.h): field-for-field match. dlmalloc.c only
 * reads dwPageSize/dwAllocationGranularity, but every field is included
 * so GetSystemInfo() writes into the correctly-sized/aligned struct. */
typedef struct _SYSTEM_INFO {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    } dummy_struct;
  } dummy_union;
  DWORD dwPageSize;
  LPVOID lpMinimumApplicationAddress;
  LPVOID lpMaximumApplicationAddress;
  DWORD_PTR dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

__declspec(dllimport) HANDLE WINAPI GetCurrentProcess(void);
__declspec(dllimport) BOOL WINAPI FlushInstructionCache(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T dwSize);
__declspec(dllimport) LPVOID WINAPI VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
__declspec(dllimport) BOOL WINAPI VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
__declspec(dllimport) SIZE_T WINAPI VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
__declspec(dllimport) void WINAPI GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
__declspec(dllimport) void WINAPI Sleep(DWORD dwMilliseconds);

/* InterlockedCompareExchange/InterlockedExchange, like
 * InterlockedCompareExchangePointer below, are real winnt.h compiler
 * intrinsics (_InterlockedCompareExchange/_InterlockedExchange, #pragma
 * intrinsic) -- NOT actual kernel32 DLL exports, discovered the hard way
 * via a real "undefined symbol: __declspec(dllimport) InterlockedCompare
 * Exchange" link error when they were declared that way here. Same fix:
 * implement via the real Clang/GCC __sync_* builtins instead. */
static __inline LONG crt_InterlockedCompareExchange(LONG_PTR_VOLATILE Destination, LONG Exchange, LONG Comparand) {
  return __sync_val_compare_and_swap(Destination, Comparand, Exchange);
}
#define InterlockedCompareExchange crt_InterlockedCompareExchange

static __inline LONG crt_InterlockedExchange(LONG_PTR_VOLATILE Target, LONG Value) {
  return __sync_lock_test_and_set(Target, Value);
}
#define InterlockedExchange crt_InterlockedExchange

/* Real winnt.h defines InterlockedCompareExchangePointer as a compiler
 * intrinsic (_InterlockedCompareExchangePointer, #pragma intrinsic), not
 * a real kernel32 export -- and it has to be, since it operates on a
 * full pointer width, unlike the always-32-bit-LONG InterlockedCompare
 * Exchange() above (dlmalloc.c's own spinlock code picks between the two
 * via #ifdef InterlockedCompareExchangePointer specifically to avoid
 * truncating a pointer-sized compare-exchange down to 32 bits on a
 * 64-bit host). No MSVC-intrinsic support is needed to provide this
 * correctly: __sync_val_compare_and_swap is a real Clang/GCC builtin
 * available on every architecture this project targets, generating the
 * same atomic compare-and-swap instruction a real intrinsic would.
 *
 * The #define (not just the function) matters: #ifdef only checks for a
 * macro, not whether a same-named function exists, so without it
 * dlmalloc.c's own #ifdef check would still see this as "not defined"
 * and fall through to the 32-bit-truncating InterlockedCompareExchange()
 * path above -- exactly matching real winnt.h's own
 * "#define InterlockedCompareExchangePointer _InterlockedCompareExchangePointer". */
static __inline PVOID crt_InterlockedCompareExchangePointer(PVOID volatile* Destination, PVOID Exchange, PVOID Comparand) {
  return __sync_val_compare_and_swap(Destination, Comparand, Exchange);
}
#define InterlockedCompareExchangePointer crt_InterlockedCompareExchangePointer
