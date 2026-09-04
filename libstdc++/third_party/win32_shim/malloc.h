/* Project-owned <malloc.h> shim -- forwards to this project's own real
 * libc malloc.h, plus two extra, real, hand-declared mingw-w64
 * extension functions.
 *
 * Windows/D3D12 Ganesh vertical slice (2026-09-04): clang's own bundled
 * resource-dir <mm_malloc.h> (lib/clang/<ver>/include/mm_malloc.h,
 * pulled in transitively by mingw-w64's own real winnt.h -> x86intrin.h
 * -> immintrin.h -> xmmintrin.h for SSE aligned-alloc intrinsics, once a
 * D3D12-consuming compile puts mingw-w64-headers on its own include
 * path -- see libcrtgfx/CMakeLists.txt's/tools/build_skia.py's own
 * comments) references __mingw_aligned_malloc()/__mingw_aligned_free(),
 * real mingw-w64 CRT extension functions -- confirmed for real
 * (2026-09-04): `use of undeclared identifier '__mingw_aligned_malloc'`
 * without them.
 *
 * A first attempt just flattened mingw-w64's own real crt/malloc.h (and
 * its own real crt/crtdefs.h -> crt/corecrt.h dependency chain) into the
 * vendored mingw-w64-headers include/ directory alongside everything
 * else -- but corecrt.h's own real wctype_t typedef (`unsigned short`)
 * directly conflicts with this project's own libc wctype_t (`unsigned
 * long`, sysroot/include/wctype.h) -- confirmed for real: `typedef
 * redefinition with different types`. This project's own libc type is
 * the one every other header/TU in this project already agrees with, so
 * mingw-w64's own malloc.h is deliberately never vendored at all (see
 * tools/fetch_mingw_w64_headers.py's own comment on this) -- this file
 * exists purely as this directory's own minimal, targeted alternative.
 *
 * -I<this directory> is ordered before mingw-w64-headers' own -I (see
 * tools/build_skia.py's/CMakeLists.txt's own comments), so this file is
 * exactly what `#include <malloc.h>` resolves to for D3D12-consuming
 * compiles too, not just this project's own, more ordinary Windows Skia
 * compiles that already relied on it before this file existed.
 *
 * #include_next steps past this file to reach this project's own real
 * libc malloc.h (found via -isystem<sysroot>/include, always on every
 * Windows Skia compile's own include path) -- so callers still see
 * every real declaration (malloc/free/calloc/realloc/...) this
 * project's own libc provides, unchanged.
 */
#ifndef CRT_WIN32_SHIM_MALLOC_H
#define CRT_WIN32_SHIM_MALLOC_H

#include_next <malloc.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Real, stable mingw-w64 CRT extension signatures (learn.microsoft.com/
 * en-us/cpp/c-runtime-library/reference/aligned-malloc, mingw-w64's own
 * real implementation just forwards to it) -- never actually called by
 * this project's own code; declared here purely so clang's own bundled
 * mm_malloc.h parses and links against *some* real declaration. */
void* __mingw_aligned_malloc(size_t _Size, size_t _Alignment);
void __mingw_aligned_free(void* _Memory);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRT_WIN32_SHIM_MALLOC_H */
