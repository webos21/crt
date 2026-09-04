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
 *
 * WINAPI_PARTITION_APP/_SYSTEM/_GAMES/_PC_APP/_PHONE_APP (2026-09-03,
 * the Windows/D3D12 Ganesh vertical slice): added for the same real
 * reason as WINAPI_PARTITION_DESKTOP above, once real Windows SDK
 * headers (shared/rpcdce.h, transitively required by <d3d12.h>/
 * <dxgi1_4.h> -- see this directory's own excpt.h top comment) started
 * getting pulled in too, via -isystem<SDK Include root>/um and /shared.
 * rpcdce.h's own real, unconditional core-type block (RPC_BINDING_
 * HANDLE/UUID/RPC_IF_HANDLE/RPC_CSTR/RPC_WSTR/...) is itself gated
 * behind `#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP |
 * WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)` -- with only
 * WINAPI_PARTITION_DESKTOP defined here, those three partition macros
 * were plain undefined identifiers, which the C preprocessor evaluates
 * as 0 inside #if -- confirmed for real (2026-09-03) this silently
 * skipped that entire block (`(0 | 0 | 0)` = false), producing
 * "unknown type name 'RPC_BINDING_HANDLE'"/'UUID'/'RPC_IF_HANDLE'/...
 * throughout rpcdce.h. Real Windows SDK winapifamily.h's own table
 * (see that header's own comment) shows WINAPI_FAMILY_DESKTOP_APP (this
 * shim's only real target, matching WINAPI_PARTITION_DESKTOP's own
 * existing reasoning above) is already a member of the APP, PC_APP, and
 * GAMES partitions too (SYSTEM/PHONE_APP are not, but making every
 * partition trivially true here is still correct for this shim's own
 * stated scope -- an ordinary native desktop program's own compile
 * should see every partition check as satisfied, matching how this
 * shim already treats WINAPI_PARTITION_DESKTOP itself; no header this
 * project actually compiles needs any partition check to read false). */
#ifndef CRT_WIN32_SHIM_WINAPIFAMILY_H
#define CRT_WIN32_SHIM_WINAPIFAMILY_H

#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_PARTITION_APP 1
#define WINAPI_PARTITION_PC_APP 1
#define WINAPI_PARTITION_PHONE_APP 1
#define WINAPI_PARTITION_SYSTEM 1
#define WINAPI_PARTITION_GAMES 1
#define WINAPI_FAMILY_PARTITION(partition) (partition)

#endif /* CRT_WIN32_SHIM_WINAPIFAMILY_H */
