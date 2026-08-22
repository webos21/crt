/* Project-owned, minimal <winerror.h> shim -- NOT a real Windows SDK header.
 *
 * libcxx's own src/system_error.cpp does `#include <winerror.h>` directly
 * (not via <windows.h>, so windows.h's own header guard/content in this
 * same directory does not help here) under _LIBCPP_WIN32API, to build its
 * own Win32-error-code -> std::errc mapping table for std::system_category().
 * Confirmed for real (2026-08-22): `fatal error: 'winerror.h' file not
 * found` building system_error.cpp -- this project's freestanding,
 * -nostdinc-shaped build has no access to the real Windows SDK's own
 * winerror.h (a huge, ~3000-line file covering the full HRESULT/FACILITY_*
 * space this project has no use for).
 *
 * Follows the same pattern as this directory's own windows.h: only the
 * exact, real, stable Win32 System Error Code constants system_error.cpp
 * itself references are declared, nothing more. These are part of the
 * permanent Win32 API stability guarantee (documented at
 * learn.microsoft.com/windows/win32/debug/system-error-codes) -- the same
 * "kernel/platform UAPI never changes" reasoning already used elsewhere in
 * this project for e.g. include/linux/futex.h's own SYS_futex/FUTEX_*
 * constants. Values below were read directly from this machine's own real
 * Windows SDK winerror.h (Windows Kits\10\Include\...\shared\winerror.h,
 * confirmed present on disk even though it is not on this recipe's own
 * include search path) rather than assumed from memory, and cross-checked
 * against libcxx/src/system_error.cpp's own real `grep -oE 'ERROR_[A-Z_0-9]+'`
 * output (49 distinct names, all covered below, no more).
 */
#ifndef CRT_WIN32_SHIM_WINERROR_H
#define CRT_WIN32_SHIM_WINERROR_H

#define ERROR_INVALID_FUNCTION 1L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_TOO_MANY_OPEN_FILES 4L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_INVALID_DRIVE 15L
#define ERROR_CURRENT_DIRECTORY 16L
#define ERROR_NOT_SAME_DEVICE 17L
#define ERROR_WRITE_PROTECT 19L
#define ERROR_BAD_UNIT 20L
#define ERROR_NOT_READY 21L
#define ERROR_SEEK 25L
#define ERROR_WRITE_FAULT 29L
#define ERROR_READ_FAULT 30L
#define ERROR_SHARING_VIOLATION 32L
#define ERROR_LOCK_VIOLATION 33L
#define ERROR_HANDLE_DISK_FULL 39L
#define ERROR_NOT_SUPPORTED 50L
#define ERROR_BAD_NETPATH 53L
#define ERROR_DEV_NOT_EXIST 55L
#define ERROR_FILE_EXISTS 80L
#define ERROR_CANNOT_MAKE 82L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_OPEN_FAILED 110L
#define ERROR_BUFFER_OVERFLOW 111L
#define ERROR_DISK_FULL 112L
#define ERROR_INVALID_NAME 123L
#define ERROR_BAD_PATHNAME 161L
#define ERROR_NEGATIVE_SEEK 131L
#define ERROR_BUSY_DRIVE 142L
#define ERROR_DIR_NOT_EMPTY 145L
#define ERROR_BUSY 170L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_LOCKED 212L
#define ERROR_DIRECTORY 267L
#define ERROR_INVALID_ACCESS 12L
#define ERROR_OUTOFMEMORY 14L
#define ERROR_BROKEN_PIPE 109L
#define ERROR_REPARSE_TAG_INVALID 4393L
#define ERROR_OPERATION_ABORTED 995L
#define ERROR_NOACCESS 998L
#define ERROR_CANTOPEN 1011L
#define ERROR_CANTREAD 1012L
#define ERROR_CANTWRITE 1013L
#define ERROR_RETRY 1237L
#define ERROR_OPEN_FILES 2401L
#define ERROR_DEVICE_IN_USE 2404L

#endif /* CRT_WIN32_SHIM_WINERROR_H */
