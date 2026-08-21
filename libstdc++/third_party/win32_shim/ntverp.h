/* Project-owned, minimal <ntverp.h> shim -- NOT a real Windows SDK header.
 * See windows.h in this same directory for the full rationale.
 *
 * LLVM libunwind's UnwindCursor.hpp includes <ntverp.h> unconditionally
 * under `#ifdef _WIN32` (top of the file), even though the one macro it
 * defines from that header (VER_PRODUCTBUILD) is only actually read inside
 * an `#if defined(_LIBUNWIND_SUPPORT_SEH_UNWIND)` block further down --
 * dead code in this project's build, since CRT builds Windows C++ with
 * -fdwarf-exceptions, not native SEH (see docs/cxx_runtime.md's "Windows
 * exception-table format: DWARF CFI, not native SEH"). The #include itself
 * still has to resolve regardless of whether the value is ever consumed.
 *
 * VER_PRODUCTBUILD's real value (verified against this machine's actual
 * installed Windows 10 SDK, C:\Program Files (x86)\Windows Kits\10\
 * Include\10.0.28000.0\shared\ntverp.h) is 10011. Defined here for
 * accuracy in case a future SEH-enabled build path ever reads it -- the
 * exact value only matters for the `VER_PRODUCTBUILD < 8000` "old Win7 SDK"
 * check in UnwindCursor.hpp, and 10011 correctly takes the modern-SDK
 * branch either way.
 */
#ifndef CRT_WIN32_SHIM_NTVERP_H
#define CRT_WIN32_SHIM_NTVERP_H

#define VER_PRODUCTBUILD 10011

#endif /* CRT_WIN32_SHIM_NTVERP_H */
