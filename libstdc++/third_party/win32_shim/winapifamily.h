/* Project-owned, minimal <winapifamily.h> shim -- NOT a real Windows SDK
 * header. See windows.h in this same directory for the full rationale.
 *
 * libcxx's own src/chrono.cpp includes this under
 * `#if _WIN32_WINNT >= _WIN32_WINNT_WIN8` -- since this project never
 * defines _WIN32_WINNT (or _WIN32_WINNT_WIN8), the C preprocessor treats
 * both as 0 in that comparison, so the #if is always true and this header
 * is always pulled in, regardless of any real target-Windows-version
 * value. chrono.cpp then checks `WINAPI_FAMILY_PARTITION(
 * WINAPI_PARTITION_DESKTOP)` to choose between GetSystemTimePreciseAs
 * FileTime (desktop apps) and GetSystemTimeAsFileTime (UWP/Store apps).
 * This project only ever builds ordinary native desktop-style Windows
 * programs (see README.md's Non-Goals -- no UWP/Store target exists),
 * so this shim makes that check unconditionally true rather than
 * reproducing the real header's full partition-bitmask machinery.
 */
#ifndef CRT_WIN32_SHIM_WINAPIFAMILY_H
#define CRT_WIN32_SHIM_WINAPIFAMILY_H

#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_FAMILY_PARTITION(partition) (partition)

#endif /* CRT_WIN32_SHIM_WINAPIFAMILY_H */
