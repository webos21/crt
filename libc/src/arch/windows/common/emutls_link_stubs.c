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

/* The emulated-TLS runtime itself (the -femulated-tls __emutls_get_address()), owned by this project rather
 * than taken from compiler-rt's emutls.c.
 *
 * compiler-rt registers an atexit() handler that frees the TLS index and the mutex when the process exits.
 * exit() runs atexit handlers while other threads are still running (they are only stopped by the final
 * ExitProcess), so a JavaScriptCore compiler thread that touched a thread_local in that window got
 * TlsGetValue() == NULL with ERROR_INVALID_PARAMETER from the freed index, and emutls' win_abort() ended the
 * process with the silent exit status 134 (about 5% of small JavaScriptCore runs, always just after the
 * script had finished). The operating system reclaims everything at exit, so nothing is torn down here.
 *
 * Layout and semantics follow compiler-rt's: a control object per thread_local {size, align, index, templ},
 * a per-thread array of pointers indexed by the control's number, each object allocated on first use from the
 * template (or zeroed). This copy is per module, as compiler-rt's was; the index state is initialised on first
 * use whatever control object comes first (compiler-rt's fast path skipped that for an already numbered one). */
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

__declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
__declspec(dllimport) void* __stdcall TlsGetValue(unsigned long index);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long index, void* value);
__declspec(dllimport) int __stdcall SwitchToThread(void);

typedef struct {
  size_t size;
  size_t align;
  uintptr_t index; /* 0: not numbered yet */
  void* templ;
} crt_emutls_control;

typedef struct {
  uintptr_t reserved;
  uintptr_t size; /* slots */
  void* data[1];
} crt_emutls_array;

static volatile long crt_emutls_state; /* 0 untouched, 1 initialising, 2 ready */
static unsigned long crt_emutls_key;
static volatile uintptr_t crt_emutls_count;

static void crt_emutls_init(void) {
  long expected = 0;

  if (__atomic_load_n(&crt_emutls_state, __ATOMIC_ACQUIRE) == 2) {
    return;
  }
  if (__atomic_compare_exchange_n(&crt_emutls_state, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    crt_emutls_key = TlsAlloc();
    if (crt_emutls_key == 0xFFFFFFFFUL) {
      abort();
    }
    __atomic_store_n(&crt_emutls_state, 2, __ATOMIC_RELEASE);
    return;
  }
  while (__atomic_load_n(&crt_emutls_state, __ATOMIC_ACQUIRE) != 2) {
    SwitchToThread();
  }
}

void* __emutls_get_address(crt_emutls_control* control) {
  uintptr_t index;
  crt_emutls_array* array;
  void** slot;

  crt_emutls_init();
  index = __atomic_load_n(&control->index, __ATOMIC_ACQUIRE);
  if (index == 0) {
    uintptr_t number = __atomic_add_fetch(&crt_emutls_count, 1, __ATOMIC_SEQ_CST);
    uintptr_t none = 0;

    __atomic_compare_exchange_n(&control->index, &none, number, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    index = __atomic_load_n(&control->index, __ATOMIC_ACQUIRE);
  }
  array = (crt_emutls_array*)TlsGetValue(crt_emutls_key);
  if (array == 0 || array->size < index) {
    uintptr_t slots = (index + 15) & ~(uintptr_t)15;
    uintptr_t old_slots = array != 0 ? array->size : 0;
    crt_emutls_array* grown = (crt_emutls_array*)realloc(array, sizeof(uintptr_t) * 2 + slots * sizeof(void*));

    if (grown == 0) {
      abort();
    }
    if (array == 0) {
      grown->reserved = 0;
    }
    memset(&grown->data[old_slots], 0, (slots - old_slots) * sizeof(void*));
    grown->size = slots;
    if (!TlsSetValue(crt_emutls_key, grown)) {
      abort();
    }
    array = grown;
  }
  slot = &array->data[index - 1];
  if (*slot == 0) {
    size_t align = control->align < sizeof(void*) ? sizeof(void*) : control->align;
    void* object = _aligned_malloc(control->size, align);

    if (object == 0) {
      abort();
    }
    if (control->templ != 0) {
      memcpy(object, control->templ, control->size);
    } else {
      memset(object, 0, control->size);
    }
    *slot = object;
  }
  return *slot;
}
