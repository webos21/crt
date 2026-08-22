/* Project-owned, minimal <windows.h> shim -- NOT a real Windows SDK header.
 *
 * This exists because a small number of LLVM libunwind source files
 * `#include <windows.h>` unconditionally under `#ifdef _WIN32`/similar
 * guards (AddressSpace.hpp's findUnwindSections(), RWMutex.hpp's internal
 * locking), which this project's freestanding, -nostdinc build cannot
 * satisfy from any host SDK. CRT deliberately does not patch libunwind's
 * own source for this (see libstdc++/third_party/libunwind/recipe.json's
 * notes) -- instead this directory is added to the include search path
 * for that one recipe's build only (tools/crt-libcxx-build.py's
 * common_cmake_args()), so the unmodified #includes resolve here.
 *
 * This follows the same pattern already used throughout
 * libc/src/arch/windows/ (see e.g. common/syscall.c): raw
 * __declspec(dllimport) prototypes and hand-declared struct layouts for
 * exactly the Win32 surface actually used, never a full/real SDK header.
 * The declarations below are scoped to precisely what those libunwind
 * source files need, no more:
 *   - GetCurrentProcess(), GetLastError(): ordinary kernel32 exports.
 *   - EnumProcessModules(): NOT linked from psapi.lib/psapi.dll. Verified
 *     directly against this machine's real kernel32.dll/psapi.dll exports
 *     (llvm-objdump -p): kernel32.dll exports K32EnumProcessModules
 *     directly (Vista+), while psapi.dll only re-exports the legacy
 *     `EnumProcessModules` name for pre-Vista compatibility. Real Windows
 *     SDK psapi.h itself `#define`s EnumProcessModules to
 *     K32EnumProcessModules for exactly this reason. This project already
 *     links kernel32 only for its Windows OS boundary (see README.md), so
 *     this shim does the same redirect rather than adding a new psapi.lib
 *     dependency.
 *   - IMAGE_DOS_HEADER/IMAGE_NT_HEADERS/IMAGE_FILE_HEADER/
 *     IMAGE_SECTION_HEADER/IMAGE_FIRST_SECTION: real, stable, publicly
 *     documented PE/COFF header layout (Microsoft's PE/COFF
 *     specification), not an internal/undocumented format. IMAGE_NT_HEADERS
 *     below deliberately omits a real OptionalHeader field body -- neither
 *     libunwind's own use nor IMAGE_FIRST_SECTION ever reads through it,
 *     only takes its address/offset, so an opaque one-byte placeholder
 *     member is enough to reproduce the correct offsetof() value for both
 *     32-bit and 64-bit PE images without declaring the full
 *     IMAGE_OPTIONAL_HEADER32/64 layout at all.
 *   - SRWLOCK and its four Acquire/Release functions: see the dedicated
 *     comment at their declaration below.
 *   - FILETIME/LARGE_INTEGER/GetSystemTimeAsFileTime/
 *     GetSystemTimePreciseAsFileTime/QueryPerformanceCounter/
 *     QueryPerformanceFrequency: libcxx's own src/chrono.cpp (not
 *     libunwind), for system_clock::now()/steady_clock::now() under
 *     _LIBCPP_WIN32API -- a legitimate, real Windows-native timing need
 *     (this project's equivalent of Linux's clock_gettime()), unlike the
 *     MSVC-UCRT locale/random surface this project's own recipe.json
 *     patches deliberately redirect away from (see libstdc++/third_party/
 *     libcxx/recipe.json's own notes). All four functions verified as
 *     real kernel32.dll exports via llvm-objdump -p; LARGE_INTEGER only
 *     declares the QuadPart member chrono.cpp actually reads, matching
 *     this file's existing IMAGE_NT_HEADERS precedent of declaring only
 *     what is used, not a full real SDK struct.
 *
 * All the real kernel32 exports below are plain C symbols (verified via
 * llvm-nm against this machine's real kernel32.lib: plain "GetLastError",
 * "K32EnumProcessModules", "AcquireSRWLockShared", etc., no C++ mangling).
 * libunwind.cpp/AddressSpace.hpp/RWMutex.hpp are C++ translation units, so
 * without an explicit extern "C" block these declarations would otherwise
 * get C++ linkage and never match -- confirmed for real: the first attempt
 * without this guard failed with "undefined symbol: __declspec(dllimport)
 * K32EnumProcessModules(void*, void**, unsigned long, unsigned long*)",
 * lld demangling a C++-mangled reference that could never resolve against
 * the plain C name actually exported by kernel32.lib.
 */
#ifndef CRT_WIN32_SHIM_WINDOWS_H
#define CRT_WIN32_SHIM_WINDOWS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;
typedef HANDLE HMODULE;
#define FALSE 0
#define TRUE 1

__declspec(dllimport) HANDLE __stdcall GetCurrentProcess(void);
__declspec(dllimport) DWORD __stdcall GetLastError(void);

typedef struct _FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME;

typedef union _LARGE_INTEGER {
  /* Real Win32 LARGE_INTEGER also exposes the low/high halves directly
   * (an anonymous nested struct aliasing the same storage as QuadPart) --
   * needed by libcxx's own src/filesystem/time_utils.h, which reads/writes
   * li.LowPart/li.HighPart when converting to/from FILETIME's own two
   * separate DWORD fields (chrono.cpp above only ever needed QuadPart).
   * Clang supports this anonymous-struct-in-union layout as a portable
   * vendor extension even without -fms-extensions (this project
   * deliberately does not add that flag project-wide, see __int64's own
   * comment above); verified this project's own build already compiles it
   * warning-clean under the -Wall/-Wextra set applied here. */
  struct {
    DWORD LowPart;
    long HighPart;
  };
  int64_t QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

__declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(FILETIME* lpSystemTimeAsFileTime);
__declspec(dllimport) void __stdcall GetSystemTimePreciseAsFileTime(FILETIME* lpSystemTimeAsFileTime);
__declspec(dllimport) BOOL __stdcall QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
__declspec(dllimport) BOOL __stdcall QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

/* __int64: a real MSVC/mingw builtin type keyword, only recognized by
 * clang under -fms-extensions -- this project deliberately does not add
 * that flag project-wide just for one type name libcxx's own chrono.cpp
 * happens to spell this way. A plain typedef is enough: on this LLP64
 * target (x86_64-w64-mingw32) __int64 is always exactly `long long`. */
typedef long long __int64;

/* SYSTEM_INFO/GetSystemInfo: libcxx's own src/thread.cpp, for
 * thread::hardware_concurrency() under _LIBCPP_WIN32API (this project's
 * equivalent of Linux's sysconf(_SC_NPROCESSORS_ONLN) -- a legitimate,
 * real Windows-native need, same category as chrono.cpp's timing APIs
 * above). Only dwNumberOfProcessors is ever read, but the full real
 * struct is declared (not trimmed to just that field): GetSystemInfo()
 * itself writes according to the REAL struct's full size, so a smaller
 * declaration here would let it write past the end of a caller's actual
 * (smaller) stack allocation. Verified as a real kernel32.dll export via
 * llvm-objdump -p. */
typedef struct _SYSTEM_INFO {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    } s;
  } u;
  DWORD dwPageSize;
  void* lpMinimumApplicationAddress;
  void* lpMaximumApplicationAddress;
  uintptr_t dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

__declspec(dllimport) void __stdcall GetSystemInfo(SYSTEM_INFO* lpSystemInfo);

/* RWMutex.hpp's own internal locking (used unconditionally under _WIN32,
 * regardless of exception model -- guards libunwind's process-wide unwind
 * table registration list). Real, stable Win32 ABI: SRWLOCK is a single
 * pointer-sized opaque field, zero-initialized (verified against this
 * machine's real winnt.h/synchapi.h: `struct _RTL_SRWLOCK { PVOID Ptr; }`,
 * `SRWLOCK_INIT` == `{0}`). The four Acquire/Release functions are real
 * kernel32.dll exports (verified via llvm-objdump -p on this machine's
 * kernel32.dll -- forwarded internally to ntdll, transparent to callers),
 * so this needs no library beyond the kernel32 this project already links
 * (see README.md). */
typedef struct _SRWLOCK {
  void* Ptr;
} SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT \
  { 0 }

__declspec(dllimport) void __stdcall AcquireSRWLockExclusive(PSRWLOCK SRWLock);
__declspec(dllimport) void __stdcall AcquireSRWLockShared(PSRWLOCK SRWLock);
__declspec(dllimport) void __stdcall ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
__declspec(dllimport) void __stdcall ReleaseSRWLockShared(PSRWLOCK SRWLock);

#define IMAGE_SIZEOF_SHORT_NAME 8

typedef struct _IMAGE_DOS_HEADER {
  /* Only e_magic and e_lfanew are ever read by findUnwindSections(); the 29
   * WORDs in between (e_cblp..e_res2, real MZ-header fields never used
   * here) are collapsed into one placeholder so e_lfanew still lands at
   * the correct, well-known fixed file offset 0x3C. */
  WORD e_magic;
  WORD e_unused[29];
  int32_t e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
  WORD Machine;
  WORD NumberOfSections;
  DWORD TimeDateStamp;
  DWORD PointerToSymbolTable;
  DWORD NumberOfSymbols;
  WORD SizeOfOptionalHeader;
  WORD Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_NT_HEADERS {
  DWORD Signature;
  IMAGE_FILE_HEADER FileHeader;
  /* Real IMAGE_OPTIONAL_HEADER32/64 deliberately not declared -- see the
   * file comment above. Only this field's own offset is ever used. */
  BYTE OptionalHeader;
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

typedef struct _IMAGE_SECTION_HEADER {
  BYTE Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    DWORD PhysicalAddress;
    DWORD VirtualSize;
  } Misc;
  DWORD VirtualAddress;
  DWORD SizeOfRawData;
  DWORD PointerToRawData;
  DWORD PointerToRelocations;
  DWORD PointerToLinenumbers;
  WORD NumberOfRelocations;
  WORD NumberOfLinenumbers;
  DWORD Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define IMAGE_FIRST_SECTION(ntheader) \
  ((PIMAGE_SECTION_HEADER)((BYTE*)(ntheader) + \
                           offsetof(IMAGE_NT_HEADERS, OptionalHeader) + \
                           (ntheader)->FileHeader.SizeOfOptionalHeader))

/* AreFileApisANSI/WideCharToMultiByte/MultiByteToWideChar/CP_ACP/CP_OEMCP/
 * MB_ERR_INVALID_CHARS: libcxx's own src/filesystem/path.cpp, for
 * path::string()/path::wstring() narrow<->wide conversions under
 * _LIBCPP_WIN32API -- confirmed for real (2026-08-22):
 * `error: no type named 'UINT' in the global namespace` (and the same for
 * AreFileApisANSI/CP_ACP/CP_OEMCP/MB_ERR_INVALID_CHARS) building path.cpp,
 * the first libcxx source file that reaches this code path. This is the
 * same category of legitimate, real Windows-native need as chrono.cpp's
 * timing APIs and thread.cpp's SYSTEM_INFO above (a real codepage
 * conversion, not the MSVC-UCRT locale/random surface this project's own
 * recipe.json patches deliberately redirect away from) -- confirmed as
 * real kernel32.dll exports via llvm-objdump -p on this machine's own
 * kernel32.dll (AreFileApisANSI, WideCharToMultiByte, MultiByteToWideChar
 * all present, ordinary plain-C exports like GetLastError/GetSystemInfo
 * above). Signatures match the real, stable, publicly documented Win32
 * API exactly (learn.microsoft.com/windows/win32/api/...). */
typedef unsigned int UINT;

#define CP_ACP 0
#define CP_OEMCP 1
#define MB_ERR_INVALID_CHARS 0x00000008

__declspec(dllimport) BOOL __stdcall AreFileApisANSI(void);
__declspec(dllimport) int __stdcall WideCharToMultiByte(UINT CodePage, DWORD dwFlags, const wchar_t* lpWideCharStr,
                                                         int cchWideChar, char* lpMultiByteStr, int cbMultiByte,
                                                         const char* lpDefaultChar, BOOL* lpUsedDefaultChar);
__declspec(dllimport) int __stdcall MultiByteToWideChar(UINT CodePage, DWORD dwFlags, const char* lpMultiByteStr,
                                                         int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRT_WIN32_SHIM_WINDOWS_H */
