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

/* Windows/D3D12 Ganesh vertical slice (2026-09-04): a second, distinct
 * real need for this shim, unrelated to libunwind/libcxx above. Skia's
 * own public include/gpu/ganesh/d3d/GrD3DTypes.h (and, transitively, the
 * project's own third_party/externals/d3d12allocator vendor checkout)
 * unconditionally #include <d3d12.h>/<dxgi1_4.h>. An earlier attempt at
 * this slice tried pointing those at the raw Microsoft Windows SDK's own
 * um/shared headers directly -- confirmed for real (2026-09-03/04) that
 * this is a real dead end, not just a missing-flag problem: d3d12.h
 * itself does `#include "windows.h"` (quoted, resolves relative to
 * d3d12.h's own SDK um/ directory first, bypassing this shim's own -I
 * priority entirely), pulling in the raw SDK's *complete* windows.h,
 * whose own winnt.h assumes real MSVC-only architecture macros
 * (_M_AMD64) *and* real MSVC-only atomic/memory-fence compiler
 * intrinsics (ReadNoFence/WriteRelease8/...) that clang only implements
 * under its `*-windows-msvc` target's own -fms-compatibility mode, not
 * this project's `--target=x86_64-w64-mingw32` one -- exactly the class
 * of problem the mingw-w64 project's own real header set exists to
 * avoid. This project now vendors that header set instead (see
 * tools/fetch_mingw_w64_headers.py, wired in by libcrtgfx/CMakeLists.txt
 * only for CRTGFX_HAVE_D3D12 Windows builds) -- it is written to compile
 * clean under plain clang/gcc, no MSVC compatibility mode needed.
 *
 * mingw-w64's own d3d12.h/dxgi1_4.h use a plain angle `#include
 * <windows.h>`, which (like every other angle include in this project's
 * Windows builds) still resolves to *this* shim first, for the same
 * real reason described above (-I always wins over -isystem, regardless
 * of position on the command line) -- so without help, D3D12 builds
 * would see only this shim's own narrow, libunwind-oriented windows.h
 * instead of mingw-w64's real, complete one. Fix: when mingw-w64's own
 * headers are ALSO on the include path (true only for the D3D12-
 * touching compiles that add them; never true for the plain libunwind/
 * libcxx bootstrap build above, which puts nothing else Windows-header-
 * shaped on its own path), #include_next steps past this file to reach
 * them, and this shim's own narrower declarations below are skipped
 * entirely -- mingw-w64's real windows.h is a strict superset of
 * everything this shim hand-declares. __has_include_next both makes
 * this a real no-op (identical behavior to before this change) for the
 * libunwind/libcxx build, and avoids a hard error on any older clang
 * that lacks the extension. winerror.h/winioctl.h/psapi.h/ntverp.h in
 * this same directory get the identical treatment, each guarding its
 * own narrower content -- see each file's own matching comment; io.h/
 * direct.h/excpt.h do not, since mingw-w64-headers/include does not
 * provide those at all (they ship in mingw-w64-crt instead, a separate,
 * not-vendored piece -- this project supplies its own libc), so there is
 * nothing to defer to for those three. */
#if defined(__has_include_next)
#if __has_include_next(<windows.h>)
#include_next <windows.h>
#else
#define CRT_WIN32_SHIM_WINDOWS_H_OWN_CONTENT 1
#endif
#else
#define CRT_WIN32_SHIM_WINDOWS_H_OWN_CONTENT 1
#endif

#ifdef CRT_WIN32_SHIM_WINDOWS_H_OWN_CONTENT

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

/* FormatMessageA/LocalFree/FORMAT_MESSAGE_*: libcxx's own src/
 * system_error.cpp, for __system_error_category::message() under
 * _LIBCPP_WIN32API -- confirmed for real (2026-08-22): `no member named
 * 'FormatMessageA'`/`use of undeclared identifier 'FORMAT_MESSAGE_
 * ALLOCATE_BUFFER'`/`...'LocalFree'` building system_error.cpp. Same
 * category as this file's other real Win32-native needs above (turning a
 * raw Win32 GetLastError() code into a human-readable string is a
 * legitimate, real Windows-native need with no portable equivalent this
 * project's own libc could substitute, unlike the MSVC-UCRT locale/random
 * surface recipe.json's own patches deliberately redirect away from).
 * Confirmed as real kernel32.dll exports via llvm-objdump -p on this
 * machine's own kernel32.dll. FORMAT_MESSAGE_* values and the signature
 * match the real, stable, publicly documented Win32 API exactly
 * (learn.microsoft.com/windows/win32/api/winbase/nf-winbase-
 * formatmessagea). */
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200

/* Arguments is really `va_list*` in the real SDK; declared here as `void*`
 * to avoid a <stdarg.h> dependency this shim otherwise has no need for --
 * ABI-identical (both are plain pointers) and system_error.cpp's own call
 * site only ever passes a literal nullptr for this parameter. */
__declspec(dllimport) unsigned long __stdcall FormatMessageA(DWORD dwFlags, const void* lpSource, DWORD dwMessageId,
                                                               DWORD dwLanguageId, char* lpBuffer, DWORD nSize,
                                                               void* Arguments);
__declspec(dllimport) HANDLE __stdcall LocalFree(HANDLE hMem);
__declspec(dllimport) void __stdcall SetLastError(DWORD dwErrCode);

/* DeviceIoControl is the only Kernel32 entry point libc++'s filesystem
 * reparse-point reader needs from <winioctl.h>.  The control-code constants
 * intentionally live in the sibling winioctl.h shim, as they do in the SDK. */
__declspec(dllimport) BOOL __stdcall DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode,
                                                     void* lpInBuffer, DWORD nInBufferSize,
                                                     void* lpOutBuffer, DWORD nOutBufferSize,
                                                     DWORD* lpBytesReturned, void* lpOverlapped);

/* Narrow file-system portion of WinBase.h used by libc++'s own Windows
 * <filesystem> backend.  These are Kernel32 APIs and documented data
 * layouts, not UCRT compatibility declarations. */
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_SHARE_READ 0x00000001UL
#define FILE_SHARE_WRITE 0x00000002UL
#define FILE_SHARE_DELETE 0x00000004UL
#define FILE_READ_ATTRIBUTES 0x00000080UL
#define FILE_WRITE_ATTRIBUTES 0x00000100UL
#define GENERIC_WRITE 0x40000000UL
#define DELETE 0x00010000UL
#define OPEN_EXISTING 3UL
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000UL
#define FILE_FLAG_OPEN_REPARSE_POINT 0x00200000UL
#define FILE_ATTRIBUTE_READONLY 0x00000001UL
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010UL
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400UL
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFUL
#define FILE_BEGIN 0UL
#define MOVEFILE_REPLACE_EXISTING 0x00000001UL
#define MOVEFILE_COPY_ALLOWED 0x00000002UL
#define MOVEFILE_WRITE_THROUGH 0x00000008UL
#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1UL
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2UL
#define ERROR_INVALID_PARAMETER 87UL
#define ERROR_PATH_NOT_FOUND 3UL
#define ERROR_NOT_ENOUGH_MEMORY 8UL
#define MAX_PATH 260
#define FILE_NAME_NORMALIZED 0x0UL
#define VOLUME_NAME_DOS 0x0UL

typedef struct _FILE_BASIC_INFO {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  DWORD FileAttributes;
} FILE_BASIC_INFO;
typedef struct _FILE_STANDARD_INFO {
  LARGE_INTEGER AllocationSize;
  LARGE_INTEGER EndOfFile;
  DWORD NumberOfLinks;
  BYTE DeletePending;
  BYTE Directory;
} FILE_STANDARD_INFO;
typedef struct _FILE_ATTRIBUTE_TAG_INFO {
  DWORD FileAttributes;
  DWORD ReparseTag;
} FILE_ATTRIBUTE_TAG_INFO;
typedef struct _FILE_DISPOSITION_INFO {
  BYTE DeleteFile;
} FILE_DISPOSITION_INFO;
typedef union _ULARGE_INTEGER {
  struct {
    DWORD LowPart;
    DWORD HighPart;
  };
  uint64_t QuadPart;
} ULARGE_INTEGER;
typedef struct _BY_HANDLE_FILE_INFORMATION {
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD dwVolumeSerialNumber;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD nNumberOfLinks;
  DWORD nFileIndexHigh;
  DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION;
typedef struct _WIN32_FIND_DATAW {
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  /* libc++ is built with Bionic's UTF-32 wchar_t.  Its filesystem code
   * consumes this member as wchar_t[], so retain that source-level type;
   * the runtime's wide-path conversion boundary remains responsible for
   * adapting it before a real Win32 call. */
  wchar_t cFileName[MAX_PATH];
  wchar_t cAlternateFileName[14];
} WIN32_FIND_DATAW;

#define FileBasicInfo 0
#define FileStandardInfo 1
#define FileDispositionInfo 4
#define FileAttributeTagInfo 9

__declspec(dllimport) HANDLE __stdcall CreateFileW(const wchar_t* lpFileName, DWORD dwDesiredAccess,
                                                     DWORD dwShareMode, void* lpSecurityAttributes,
                                                     DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                                     HANDLE hTemplateFile);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE hObject);
__declspec(dllimport) BOOL __stdcall GetFileInformationByHandleEx(HANDLE hFile, int FileInformationClass,
                                                                    void* lpFileInformation, DWORD dwBufferSize);
__declspec(dllimport) BOOL __stdcall GetFileInformationByHandle(HANDLE hFile,
                                                                  BY_HANDLE_FILE_INFORMATION* lpFileInformation);
__declspec(dllimport) BOOL __stdcall SetFileInformationByHandle(HANDLE hFile, int FileInformationClass,
                                                                  const void* lpFileInformation, DWORD dwBufferSize);
__declspec(dllimport) BOOL __stdcall CreateDirectoryW(const wchar_t* lpPathName, void* lpSecurityAttributes);
__declspec(dllimport) BOOL __stdcall CreateSymbolicLinkW(const wchar_t* lpSymlinkFileName,
                                                          const wchar_t* lpTargetFileName, DWORD dwFlags);
__declspec(dllimport) BOOL __stdcall CreateHardLinkW(const wchar_t* lpFileName,
                                                      const wchar_t* lpExistingFileName,
                                                      void* lpSecurityAttributes);
__declspec(dllimport) BOOL __stdcall SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
                                                        LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);
__declspec(dllimport) BOOL __stdcall SetEndOfFile(HANDLE hFile);
__declspec(dllimport) BOOL __stdcall SetFileTime(HANDLE hFile, const FILETIME* lpCreationTime,
                                                  const FILETIME* lpLastAccessTime,
                                                  const FILETIME* lpLastWriteTime);
__declspec(dllimport) BOOL __stdcall MoveFileExW(const wchar_t* lpExistingFileName,
                                                  const wchar_t* lpNewFileName, DWORD dwFlags);
__declspec(dllimport) BOOL __stdcall SetCurrentDirectoryW(const wchar_t* lpPathName);
__declspec(dllimport) DWORD __stdcall GetCurrentDirectoryW(DWORD nBufferLength, wchar_t* lpBuffer);
__declspec(dllimport) DWORD __stdcall GetFinalPathNameByHandleW(HANDLE hFile, wchar_t* lpszFilePath,
                                                                  DWORD cchFilePath, DWORD dwFlags);
__declspec(dllimport) DWORD __stdcall GetFileAttributesW(const wchar_t* lpFileName);
__declspec(dllimport) BOOL __stdcall SetFileAttributesW(const wchar_t* lpFileName, DWORD dwFileAttributes);
__declspec(dllimport) BOOL __stdcall GetDiskFreeSpaceExW(const wchar_t* lpDirectoryName,
                                                          ULARGE_INTEGER* lpFreeBytesAvailableToCaller,
                                                          ULARGE_INTEGER* lpTotalNumberOfBytes,
                                                          ULARGE_INTEGER* lpTotalNumberOfFreeBytes);
__declspec(dllimport) HANDLE __stdcall FindFirstFileW(const wchar_t* lpFileName,
                                                       WIN32_FIND_DATAW* lpFindFileData);
__declspec(dllimport) BOOL __stdcall FindNextFileW(HANDLE hFindFile,
                                                   WIN32_FIND_DATAW* lpFindFileData);
__declspec(dllimport) BOOL __stdcall FindClose(HANDLE hFindFile);
__declspec(dllimport) DWORD __stdcall GetTempPathW(DWORD nBufferLength, wchar_t* lpBuffer);

/* _get_osfhandle: libcxx's own src/filesystem/posix_compat.h calls this
 * real MSVC-CRT function three times (fstat/ftruncate/fchmod's own
 * fd-taking overloads) to recover the real Windows HANDLE behind an
 * already-open fd -- confirmed for real (2026-08-22): `fatal error:
 * 'io.h' file not found` compiling directory_entry.cpp/directory_
 * iterator.cpp/operations.cpp (posix_compat.h's own #include <io.h>
 * under _LIBCPP_WIN32API). Unlike the other MSVC-CRT gaps this recipe's
 * own patches route around by dropping/redirecting the feature
 * (std::print's WriteConsoleW fast path, fstream's C++26 native_handle()
 * -- both real but skippable), fstat/ftruncate/fchmod are core,
 * unavoidable <filesystem> operations (std::filesystem::resize_file,
 * ::permissions, status queries against an already-open fd) -- worth a
 * real implementation instead of a third degradation. This project's own
 * fd model already has exactly the needed fd->HANDLE mapping internally
 * (libc/src/arch/windows/common/syscall.c's own private get_fd_handle(),
 * already used by e.g. isatty()) -- __crt_windows_fd_get_handle() (see
 * its own declaration in libc/include/private/crt_fd_table.h) exposes it
 * publicly for exactly this. A real MSVC _get_osfhandle() can also
 * return -1 (INTPTR_MAX-adjacent sentinel, not just any negative value)
 * on failure; __crt_windows_fd_get_handle() already returns plain 0 for
 * "no real handle" (a null HANDLE reads the same way to every caller in
 * this file, all of which only ever check the result via
 * reinterpret_cast<HANDLE>(...) before an ordinary Win32 call that
 * itself rejects a null/invalid HANDLE), so no extra sentinel-translation
 * is needed here. */
extern uintptr_t __crt_windows_fd_get_handle(int fd);
static inline intptr_t _get_osfhandle(int fd) {
  return (intptr_t)__crt_windows_fd_get_handle(fd);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRT_WIN32_SHIM_WINDOWS_H_OWN_CONTENT */

#endif /* CRT_WIN32_SHIM_WINDOWS_H */
