/* Link-time-only stubs for two real UCRT-internal entry points that
 * compiler-rt's own emutls.c (built into clang_rt.builtins-x86_64.lib,
 * needed for __emutls_get_address -- see tools/crt-cc's own -femulated-tls
 * comment) references from its own internal win_error() diagnostic path.
 *
 * -femulated-tls lowers __declspec(thread)/thread_local to
 * __emutls_get_address() calls, implemented (per real, vendor-shipped
 * compiler-rt) on top of the same TlsAlloc()/TlsGetValue()/TlsSetValue()
 * Win32 APIs this project's own libc/src/tls.c already uses for its own,
 * separate, project-owned dynamic-TLS mechanism. That implementation's own
 * win_error() helper -- reached only if one of those Win32 calls
 * unexpectedly fails, an essentially unreachable condition in practice --
 * formats a diagnostic message through __stdio_common_vfprintf() (the real
 * UCRT's own internal vfprintf) against a FILE* obtained from
 * __acrt_iob_func() (the real UCRT's own internal stdin/stdout/stderr
 * accessor). Neither exists in this project's freestanding CRT, which
 * never links a real UCRT at all -- confirmed for real (2026-08-23):
 * `ld.lld: error: undefined symbol: __acrt_iob_func` / `__stdio_common_
 * vfprintf` the first time anything actually pulled emutls.c.obj into a
 * real link (crtgfx_skia_raster_smoke, via Skia's own SkStrikeCache::
 * GlobalStrikeCache()).
 *
 * These stubs make no attempt at a real, UCRT-ABI-compatible
 * implementation -- there is no reason to: win_error()'s own call site is
 * already a fatal-error path (a Win32 TLS API failing outright), so
 * aborting immediately, without even touching whatever arguments the
 * (mismatched-signature, deliberately parameterless here) caller set up,
 * is both safe -- the x86_64 Windows calling convention never requires a
 * callee to consume/clean up arguments it doesn't use -- and exactly as
 * useful in practice as trying to actually format and print a message
 * through infrastructure this project deliberately does not have. Kept as
 * a small, explicitly-scoped source file (only compiled directly into
 * consumers that actually need -femulated-tls-linked code, not folded into
 * the regular c/c_shared library sources) rather than a real printf-style
 * implementation.
 */

extern void abort(void);

void __acrt_iob_func(void) {
  abort();
}

void __stdio_common_vfprintf(void) {
  abort();
}
