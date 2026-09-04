/* Project-owned, minimal <excpt.h> shim -- NOT a real Windows SDK/MSVC
 * CRT header.
 *
 * Windows/D3D12 Ganesh vertical slice (2026-09-03): the real Windows
 * SDK's own shared/rpc.h (transitively required by <d3d12.h>/
 * <dxgi1_4.h>, which Skia's own public include/gpu/ganesh/d3d/
 * GrD3DTypes.h and the project's own third_party/externals/
 * d3d12allocator vendor checkout both unconditionally #include)
 * unconditionally `#include <excpt.h>` itself -- confirmed for real
 * (2026-09-03, read directly): this happens even when rpc.h's own
 * earlier, separate `#include <windows.h>` gate is satisfied by this
 * project's own minimal windows.h shim (see the sibling windows.h in
 * this directory), so the fix cannot be "let the real Windows.h
 * through" -- rpc.h reaches for excpt.h unconditionally on its own,
 * independent of whichever windows.h a caller provided.
 *
 * <excpt.h> is a genuine Visual C++ CRT header (ships with the MSVC
 * toolset's own include dir), not part of the Windows SDK proper -- this
 * project deliberately never vendors the MSVC CRT/UCRT (see this
 * directory's own windows.h top comment, and libstdc++/third_party/
 * libcxx/recipe.json's notes on routing around MSVC-UCRT-only surface
 * generally). Rather than pull in a real MSVC installation for one
 * header, this shim declares exactly the real, stable, publicly
 * documented Win32 structured-exception-handling (SEH) surface
 * (learn.microsoft.com/en-us/cpp/cpp/excpt-h) that real Windows SDK
 * headers reference -- matching this directory's own established
 * "minimal, real-signature shim, no more than what's actually used"
 * discipline (see windows.h's own top comment). This project never
 * itself uses `__try`/`__except`/`__finally` (no SEH anywhere in its own
 * code, `-fno-exceptions` throughout) -- these declarations exist purely
 * so real SDK headers that mention them (rpc.h's own RpcTryExcept/
 * RpcExcept macro bodies, never actually invoked by this project's own
 * D3D12 device/command-queue/adapter code or Ganesh's own D3D backend)
 * parse cleanly; none of the functions declared below are ever actually
 * called or linked by this project.
 */
#ifndef CRT_WIN32_SHIM_EXCPT_H
#define CRT_WIN32_SHIM_EXCPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Real, stable, publicly documented values (learn.microsoft.com/en-us/
 * cpp/cpp/excpt-h) -- the three legal return values for a `__except`
 * filter expression. */
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)

/* Real, stable RPC_S_* / SEH disposition enum (used by RPC's own real
 * exception-filter callback signatures elsewhere in rpc.h/rpcdce.h) --
 * transcribed verbatim from Microsoft's own public documentation. */
typedef enum _EXCEPTION_DISPOSITION {
  ExceptionContinueExecution,
  ExceptionContinueSearch,
  ExceptionNestedException,
  ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

/* Real, stable MSVC CRT intrinsics backing GetExceptionCode()/
 * GetExceptionInformation()/AbnormalTermination() inside a real
 * `__except` filter expression -- declared (not defined) here purely so
 * any real SDK header text that references the macro names below parses;
 * never actually called by this project (no `__except` anywhere in its
 * own code). */
unsigned long __cdecl _exception_code(void);
void *__cdecl _exception_info(void);
int __cdecl _abnormal_termination(void);

#define GetExceptionCode _exception_code
#define GetExceptionInformation _exception_info
#define AbnormalTermination _abnormal_termination

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRT_WIN32_SHIM_EXCPT_H */
