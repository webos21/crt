/* __cxa_atexit()/__cxa_finalize()/__dso_handle -- Itanium C++ ABI static-
 * duration destructor registration (Itanium C++ ABI 3.3.5). Real glibc and
 * real Bionic both put these in libc itself, not in libstdc++/libc++abi:
 * confirmed for real by reading LLVM libcxxabi's own src/ tree (fetched by
 * crt-libcxx-fetch under each build preset's own out dir, external slash
 * llvm-runtimes slash libcxxabi) -- it has
 * no __cxa_atexit.cpp at all, and its own cxa_exception.cpp/private_typeinfo
 * etc only ever *call* __cxa_atexit()/__cxa_finalize(), matching a hard
 * platform-libc dependency, not a symbol libcxxabi provides.
 *
 * This project's own libc/src/exit.c already assumed exactly this shape --
 * exit() has called `__cxa_finalize(0)` through a weak reference since
 * before this file existed, so a C-only link (no C++ anywhere) still
 * builds. Before this file, the only real definition of these three
 * symbols anywhere in this project was libstdc++/src/cxxabi.c, part of the
 * bootstrap `cxx`/`cxx_shared` targets -- fine for that bootstrap ABI shim
 * on its own, but it left the imported-libc++ path (libstdc++/third_party/
 * {libcxx,libcxxabi,libunwind}, driven by crt-libcxx-build) with no
 * provider at all: libc++.so/libc++abi.so call __cxa_atexit()/
 * __cxa_thread_atexit()'s own fallback path (see below) expecting the
 * platform libc to supply it, exactly like real glibc/Bionic, and got an
 * undefined reference instead (see HISTORY.md's 2026-08-21 entry).
 *
 * Scoped to Linux only for now, matching CRT_LINUX_AUXV_FILE/
 * CRT_LINUX_TERMIOS_FILE in libc/CMakeLists.txt: macOS already gets a real
 * __cxa_atexit() from libSystem.dylib (see exit.c's own comment), and
 * Windows' own crt-libcxx-build gaps are separate, still-open, tracked
 * work (TODO.md) this file does not touch. libstdc++/src/cxxabi.c keeps
 * its own copy of these three symbols for macOS/Windows' bootstrap `cxx`/
 * `cxx_shared` targets, guarded out on Linux (see that file's own comment)
 * now that Linux gets a real one here instead -- both copies existing for
 * Linux at once would be a genuine multiple-definition conflict the first
 * time a single link pulled in both libc.a and the bootstrap cxx.a (any
 * C++ program using function-local statics needs cxxabi.c's
 * __cxa_guard_*, which would drag its __cxa_atexit copy in right
 * alongside).
 *
 * __cxa_thread_atexit_impl() (the Bionic API 23+ extension covering
 * thread_local destructors) is deliberately NOT implemented here: it needs
 * real integration with this project's own pthread thread-exit path, a
 * separate, larger PAL tranche. libstdc++/third_party/libcxxabi/
 * recipe.json instead forces LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF on
 * Linux, which is not a workaround -- libcxxabi's own config-ix.cmake
 * probe for this (`check_library_exists(c __cxa_thread_atexit_impl ...)`)
 * cannot ever return a real answer against this project's toolchain
 * (CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY means the probe only
 * compiles a .o and archives it, never actually links, so it always
 * reports "found" regardless of the true answer -- confirmed for real via
 * CMakeConfigureLog.yaml and by reproducing the identical probe by hand
 * through tools/crt-cc, which genuinely fails to link). Forcing it OFF
 * supplies the correct, verified-by-hand answer the probe cannot reach,
 * and lets libcxxabi fall back to its own portable, already-implemented,
 * __cxa_atexit()-and-TLS-key-based thread_local destructor path (see
 * cxa_thread_atexit.cpp's own comment for that path's documented, accepted
 * limitations -- unloading a dlclose()'d DSO before its thread_locals run,
 * etc. -- which are exactly the tradeoffs of running without the Bionic
 * extension, not something this project introduced). */

#include <private/crt_atomic.h>

#define CRT_CXA_ATEXIT_MAX 128

typedef void (*crt_cxa_destructor_t)(void*);

typedef struct {
  crt_cxa_destructor_t destructor;
  void* object;
  void* dso;
  int called;
} crt_cxa_atexit_entry;

static crt_spinlock crt_cxa_atexit_lock = CRT_SPINLOCK_INIT;
static crt_cxa_atexit_entry crt_cxa_atexit_entries[CRT_CXA_ATEXIT_MAX];
static int crt_cxa_atexit_count;

/* Hidden visibility: __dso_handle identifies *this* DSO's own image, not
 * some other one. libc.a is a static archive re-embedded into whichever
 * final image links it (an executable, or libc.so itself, or any other
 * DSO that links libc.a directly, e.g. libdl.a -- see libdl/CMakeLists.txt
 * -- also statically pulls in `c`), so each consumer already gets its own
 * private copy at link time; hidden visibility keeps the dynamic linker
 * from ever merging/interposing those separate copies across DSOs, which
 * would defeat the whole point (matches the exact "one specific DSO/
 * shared-object image" reasoning HISTORY.md's macOS libcxx __dso_handle
 * entry already worked through for the imported-libc++ build). */
__attribute__((visibility("hidden"))) void* __dso_handle = &__dso_handle;

int __cxa_atexit(crt_cxa_destructor_t destructor, void* object, void* dso) {
  int result = 0;

  if (destructor == 0) {
    return -1;
  }

  crt_spin_lock(&crt_cxa_atexit_lock);
  if (crt_cxa_atexit_count >= CRT_CXA_ATEXIT_MAX) {
    result = -1;
  } else {
    crt_cxa_atexit_entry* entry = &crt_cxa_atexit_entries[crt_cxa_atexit_count++];
    entry->destructor = destructor;
    entry->object = object;
    entry->dso = dso;
    entry->called = 0;
  }
  crt_spin_unlock(&crt_cxa_atexit_lock);

  return result;
}

void __cxa_finalize(void* dso) {
  int index;

  for (;;) {
    crt_cxa_destructor_t destructor = 0;
    void* object = 0;

    crt_spin_lock(&crt_cxa_atexit_lock);
    for (index = crt_cxa_atexit_count - 1; index >= 0; --index) {
      crt_cxa_atexit_entry* entry = &crt_cxa_atexit_entries[index];
      if (!entry->called && (dso == 0 || entry->dso == dso)) {
        entry->called = 1;
        destructor = entry->destructor;
        object = entry->object;
        break;
      }
    }
    crt_spin_unlock(&crt_cxa_atexit_lock);

    if (destructor == 0) {
      return;
    }
    destructor(object);
  }
}
