/* Project-owned, minimal <psapi.h> shim -- NOT a real Windows SDK header.
 * See windows.h in this same directory for the full rationale; this file
 * covers the one symbol LLVM libunwind's AddressSpace.hpp needs from
 * <psapi.h>: EnumProcessModules().
 *
 * Real Windows SDK psapi.h itself #defines EnumProcessModules to
 * K32EnumProcessModules when targeting Vista or later, specifically so
 * callers link against kernel32 (which exports K32EnumProcessModules
 * directly) instead of psapi.lib/psapi.dll (which only re-exports the
 * plain `EnumProcessModules` name for pre-Vista compatibility). Verified
 * directly against this machine's real kernel32.dll/psapi.dll exports via
 * `llvm-objdump -p`. This project already links kernel32 only for its
 * Windows OS boundary (see README.md), so this shim does the same
 * redirect rather than adding a new psapi.lib dependency.
 *
 * extern "C": see windows.h's own matching comment -- this is included
 * from libunwind.cpp (a C++ TU), so this declaration needs C linkage to
 * match kernel32.lib's plain C export name.
 */
#ifndef CRT_WIN32_SHIM_PSAPI_H
#define CRT_WIN32_SHIM_PSAPI_H

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) BOOL __stdcall K32EnumProcessModules(
    HANDLE hProcess, HMODULE* lphModule, DWORD cb, DWORD* lpcbNeeded);

#ifdef __cplusplus
} /* extern "C" */
#endif

#define EnumProcessModules K32EnumProcessModules

#endif /* CRT_WIN32_SHIM_PSAPI_H */
