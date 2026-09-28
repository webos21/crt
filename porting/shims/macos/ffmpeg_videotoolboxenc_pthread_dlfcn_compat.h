/* macOS-only compatibility shim for porting/recipes/ffmpeg.json's own
 * libavcodec/videotoolboxenc.o build (2026-09-28, "Encode and capture"
 * Tranche 4B). Force-included (via that recipe's own per-object CFLAGS
 * override, together with -D_DLFCN_H_) ahead of videotoolboxenc.c's own
 * body.
 *
 * videotoolboxenc.c is the first real-Apple-SDK-needing FFmpeg file in
 * this project (unlike its siblings videotoolbox.c/hwcontext_videotoolbox.c,
 * both plain CoreFoundation/CoreMedia/CoreVideo/VideoToolbox consumers)
 * that also directly uses this project's own pthread/dl surface for real:
 * a file-scope `static pthread_once_t once_ctrl` guarding lazy compat-
 * symbol loading via `dlsym(RTLD_DEFAULT, ...)`, plus a real per-encoder-
 * instance `pthread_mutex_t lock`/`pthread_cond_t cv_sample_sent` pair
 * used to hand frames to VTCompressionSession's own asynchronous output
 * callback.
 *
 * The pthread problem, and what does NOT work
 * ---------------------------------------------
 * Confirmed for real (2026-09-28) that simply letting this file see the
 * real Apple SDK's own <pthread.h> (exactly like its two siblings, which
 * never touch pthread at all) produces a real, reproduced infinite spin:
 * the actual pthread_once() *implementation* this project links into
 * every one of its own binaries is always this project's own
 * (libc/src/pthread.c's own plain int-backed crt_once state) -- real
 * Apple libSystem's own pthread_once() is never linked -- but Apple's
 * real pthread_once_t is a large opaque struct with its own real,
 * non-zero PTHREAD_ONCE_INIT magic signature, an ABI/value mismatch this
 * project's own crt_once state machine spins forever trying to resolve.
 *
 * Three more direct fixes were tried and rejected, all confirmed for
 * real:
 *
 * 1) Pre-defining the real SDK's own <pthread.h> top-level include guard
 *    (-D_PTHREAD_H) so this file's own #include line becomes a no-op,
 *    then force-including this project's own real include/pthread.h
 *    instead: this project's own pthread.h itself #includes <sys/
 *    types.h>, which -- still resolving via the real-Apple-SDK-first
 *    search order this one compile also needs for CoreMedia/CoreVideo/
 *    VideoToolbox -- pulls in real Apple's own sys/_pthread/
 *    _pthread_once_t.h regardless, independently of <pthread.h>'s own
 *    guard.
 * 2) Declaring competing pthread_once_t/pthread_mutex_t/pthread_cond_t
 *    typedefs directly in this shim: CoreFoundation's own header chain
 *    (CFRunLoop.h and friends) independently pulls in real Apple's own
 *    sys/_pthread/_pthread_{once,mutex,cond}_t.h on its own, with their
 *    own separate include guards untouched by anything above -- so real
 *    Apple's pthread_once_t/pthread_mutex_t/pthread_cond_t are
 *    unavoidably defined in this translation unit regardless of what
 *    this shim does to <pthread.h> itself, and a competing typedef here
 *    hits a real "typedef redefinition with different types" error
 *    against real Apple's own headers.
 * 3) A blanket "prefer this project's own sysroot for this whole
 *    compile" order swap: broke CoreFoundation's own real header chain
 *    outright (real Apple <malloc/_malloc.h>'s own __sized_by_or_null
 *    annotations, transitively required via <CoreFoundation/CFBase.h>,
 *    and real Apple <mach/arm/vm_types.h>'s own __kernel_ptr_semantics,
 *    are not something this project's own <sys/cdefs.h> has any reason
 *    to define) -- mixing this project's own libc headers into the
 *    middle of a real Apple SDK header chain broke that chain with real,
 *    unrelated parse errors.
 *
 * The actual pthread fix
 * -----------------------
 * Given real Apple's pthread_once_t/pthread_mutex_t/pthread_cond_t are
 * unavoidably what `once_ctrl`/`lock`/`cv_sample_sent` really are laid
 * out as in this translation unit, this shim leaves <pthread.h> and
 * every type/declaration it defines completely untouched (no guard
 * games, no competing typedefs, no macro renaming of the call sites --
 * a plain object/function-like #define was also tried and rejected: it
 * corrupts the real SDK's OWN later declaration of e.g. `int
 * pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);`,
 * since that declaration's own identifier is textually indistinguishable
 * from a call site to the preprocessor). Instead this shim uses `#pragma
 * redefine_extname` (a real, standard Clang/Sun/AIX-lineage pragma,
 * confirmed supported here) BEFORE <pthread.h> is ever reached, mapping
 * each of the 9 real pthread symbols this file calls (confirmed by
 * reading videotoolboxenc.c directly: pthread_once, pthread_mutex_
 * {init,destroy,lock,unlock}, pthread_cond_{init,destroy,signal,wait}) to
 * a distinct real, ordinarily-named function this shim defines below.
 * This changes only which *symbol* the compiler emits for a call to
 * e.g. `pthread_once(...)` -- type-checking still happens normally
 * against <pthread.h>'s own real declaration, so no declaration is ever
 * textually altered or duplicated. The redefine_extname target names are
 * given with their platform name-mangling already applied (a leading
 * underscore on Mach-O/arm64, confirmed for real: Clang treats a
 * redefine_extname target as a raw, already-mangled asm label, NOT a
 * plain C identifier that itself gets mangled again -- an ordinary
 * function *definition* using the same plain name, e.g. `int
 * crt_vtenc_real_pthread_once(...)`, always emits `_crt_vtenc_real_
 * pthread_once` normally, so the redefine_extname target string must
 * already carry that same leading underscore to match).
 *
 * Each real wrapper function resolves and calls through the REAL Apple
 * implementation in /usr/lib/system/libsystem_pthread.dylib, via this
 * project's own dlopen()/dlsym() (libdl/src/arch/macos/dl_macos.c) -- a
 * specific-image dlopen()+dlsym() lookup is a runtime, by-name/by-image
 * operation, completely orthogonal to the static link-time symbol
 * resolution that would otherwise always prefer this project's own
 * same-named libc.a symbols for a plain direct call. Since `once_ctrl`/
 * `lock`/`cv_sample_sent` really are Apple-shaped memory here, only
 * Apple's own real implementations can safely interpret them -- this
 * project's own pthread_once()/pthread_mutex_*()/pthread_cond_*() never
 * see these particular call sites at all now, sidestepping the ABI
 * mismatch entirely instead of fighting it.
 *
 * Verified with a standalone probe first (this project's own established
 * discipline before landing an undocumented technique): a real
 * redefine_extname + dlopen()+dlsym() against /usr/lib/system/
 * libsystem_pthread.dylib, exercising all 9 functions against real
 * Apple-typed pthread_once_t/pthread_mutex_t/pthread_cond_t objects
 * declared via a real <CoreMedia/CoreMedia.h>+<VideoToolbox/
 * VideoToolbox.h>+<pthread.h> include set (mirroring videotoolboxenc.c's
 * own real includes), ran the once-callback exactly once and correctly
 * locked/unlocked/signaled/waited/destroyed, with no hang and no crash.
 *
 * The dlfcn/RTLD_DEFAULT problem
 * -------------------------------
 * dlsym(RTLD_DEFAULT, ...) for this file's OWN unrelated lazy compat-
 * symbol loading (kVTProfileLevel_*, CFStringRef constants living in the
 * VideoToolbox/CoreMedia frameworks) is a separate, narrower problem:
 * real Apple's own RTLD_DEFAULT is (void*)-2 vs. this project's own
 * (void*)0 (libdl/src/arch/macos/dl_macos.c's own crt_dl_backend_sym()),
 * and passing -2 into this project's own dlsym() misinterprets it as a
 * real image-handle pointer and crashes -- confirmed for real with a
 * standalone probe (a real, reproduced SIGSEGV). <dlfcn.h> has no opaque
 * struct type to fight over (RTLD_DEFAULT is a plain #define, dlsym() a
 * plain function declaration), so unlike <pthread.h> above, guard-
 * blocking the real SDK's own <dlfcn.h> (via -D_DLFCN_H_, this recipe's
 * own CFLAGS override) and substituting this project's own declarations
 * below works cleanly, with no nested-header fallout -- confirmed
 * directly (this half of the shim never produced a single compile error
 * in any attempt above, and the standalone probe's own dlsym(RTLD_
 * DEFAULT, ...) call, once this half was in place, correctly found a
 * real CoreMedia symbol with no crash).
 */
#ifndef CRT_PORTING_SHIMS_MACOS_FFMPEG_VIDEOTOOLBOXENC_PTHREAD_DLFCN_COMPAT_H
#define CRT_PORTING_SHIMS_MACOS_FFMPEG_VIDEOTOOLBOXENC_PTHREAD_DLFCN_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* -- dlfcn.h subset (matches include/dlfcn.h exactly; real SDK's own
 * <dlfcn.h> is blocked via this recipe's own -D_DLFCN_H_ CFLAGS
 * override) -- */

#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_LOCAL 0x00000
#define RTLD_GLOBAL 0x00100
#define RTLD_DEFAULT ((void*)0)
#define RTLD_NEXT ((void*)-1)
void* dlopen(const char* filename, int flags);
void* dlsym(void* handle, const char* symbol);
int dlclose(void* handle);
char* dlerror(void);

/* -- pthread.h: NOT blocked, NOT redeclared here (see this file's own
 * top comment for why). Real Apple's own <pthread.h>, reached normally
 * later in videotoolboxenc.c, still applies for every type. Only the 9
 * function symbols that file actually calls are redirected, via #pragma
 * redefine_extname below, to the real-implementation wrappers this shim
 * defines. */

#pragma redefine_extname pthread_once _crt_vtenc_real_pthread_once
#pragma redefine_extname pthread_mutex_init _crt_vtenc_real_pthread_mutex_init
#pragma redefine_extname pthread_mutex_destroy _crt_vtenc_real_pthread_mutex_destroy
#pragma redefine_extname pthread_mutex_lock _crt_vtenc_real_pthread_mutex_lock
#pragma redefine_extname pthread_mutex_unlock _crt_vtenc_real_pthread_mutex_unlock
#pragma redefine_extname pthread_cond_init _crt_vtenc_real_pthread_cond_init
#pragma redefine_extname pthread_cond_destroy _crt_vtenc_real_pthread_cond_destroy
#pragma redefine_extname pthread_cond_signal _crt_vtenc_real_pthread_cond_signal
#pragma redefine_extname pthread_cond_wait _crt_vtenc_real_pthread_cond_wait

typedef void (*crt_vtenc_once_fn)(void);
typedef int (*crt_vtenc_pthread_once_fn)(void*, crt_vtenc_once_fn);
typedef int (*crt_vtenc_pthread_init_fn)(void*, const void*);
typedef int (*crt_vtenc_pthread_op_fn)(void*);
typedef int (*crt_vtenc_pthread_cond_wait_fn)(void*, void*);

/* Resolved lazily, once, from the real system pthread implementation --
 * never from this project's own libc.a (a plain direct call would always
 * link against this project's own same-named symbol instead; dlopen()+
 * dlsym() against this one specific, always-loaded system image is what
 * actually reaches Apple's own real implementation). */
static struct {
  crt_vtenc_pthread_once_fn once;
  crt_vtenc_pthread_init_fn mutex_init;
  crt_vtenc_pthread_op_fn mutex_destroy;
  crt_vtenc_pthread_op_fn mutex_lock;
  crt_vtenc_pthread_op_fn mutex_unlock;
  crt_vtenc_pthread_init_fn cond_init;
  crt_vtenc_pthread_op_fn cond_destroy;
  crt_vtenc_pthread_op_fn cond_signal;
  crt_vtenc_pthread_cond_wait_fn cond_wait;
  int resolved;
} crt_vtenc_real_pthread;

static inline void crt_vtenc_real_pthread_resolve(void) {
  void* image;
  if (crt_vtenc_real_pthread.resolved) {
    return;
  }
  image = dlopen("/usr/lib/system/libsystem_pthread.dylib", RTLD_NOW);
  crt_vtenc_real_pthread.once = (crt_vtenc_pthread_once_fn)dlsym(image, "pthread_once");
  crt_vtenc_real_pthread.mutex_init = (crt_vtenc_pthread_init_fn)dlsym(image, "pthread_mutex_init");
  crt_vtenc_real_pthread.mutex_destroy = (crt_vtenc_pthread_op_fn)dlsym(image, "pthread_mutex_destroy");
  crt_vtenc_real_pthread.mutex_lock = (crt_vtenc_pthread_op_fn)dlsym(image, "pthread_mutex_lock");
  crt_vtenc_real_pthread.mutex_unlock = (crt_vtenc_pthread_op_fn)dlsym(image, "pthread_mutex_unlock");
  crt_vtenc_real_pthread.cond_init = (crt_vtenc_pthread_init_fn)dlsym(image, "pthread_cond_init");
  crt_vtenc_real_pthread.cond_destroy = (crt_vtenc_pthread_op_fn)dlsym(image, "pthread_cond_destroy");
  crt_vtenc_real_pthread.cond_signal = (crt_vtenc_pthread_op_fn)dlsym(image, "pthread_cond_signal");
  crt_vtenc_real_pthread.cond_wait = (crt_vtenc_pthread_cond_wait_fn)dlsym(image, "pthread_cond_wait");
  crt_vtenc_real_pthread.resolved = 1;
}

/* Explicit prototypes ahead of each definition below: FFmpeg's own build
 * compiles with -Wmissing-prototypes -Werror, which would otherwise
 * reject a non-static function definition with no prior declaration. */
int crt_vtenc_real_pthread_once(void* ctrl, crt_vtenc_once_fn fn);
int crt_vtenc_real_pthread_mutex_init(void* mutex, const void* attr);
int crt_vtenc_real_pthread_mutex_destroy(void* mutex);
int crt_vtenc_real_pthread_mutex_lock(void* mutex);
int crt_vtenc_real_pthread_mutex_unlock(void* mutex);
int crt_vtenc_real_pthread_cond_init(void* cond, const void* attr);
int crt_vtenc_real_pthread_cond_destroy(void* cond);
int crt_vtenc_real_pthread_cond_signal(void* cond);
int crt_vtenc_real_pthread_cond_wait(void* cond, void* mutex);

int crt_vtenc_real_pthread_once(void* ctrl, crt_vtenc_once_fn fn) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.once(ctrl, fn);
}
int crt_vtenc_real_pthread_mutex_init(void* mutex, const void* attr) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.mutex_init(mutex, attr);
}
int crt_vtenc_real_pthread_mutex_destroy(void* mutex) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.mutex_destroy(mutex);
}
int crt_vtenc_real_pthread_mutex_lock(void* mutex) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.mutex_lock(mutex);
}
int crt_vtenc_real_pthread_mutex_unlock(void* mutex) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.mutex_unlock(mutex);
}
int crt_vtenc_real_pthread_cond_init(void* cond, const void* attr) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.cond_init(cond, attr);
}
int crt_vtenc_real_pthread_cond_destroy(void* cond) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.cond_destroy(cond);
}
int crt_vtenc_real_pthread_cond_signal(void* cond) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.cond_signal(cond);
}
int crt_vtenc_real_pthread_cond_wait(void* cond, void* mutex) {
  crt_vtenc_real_pthread_resolve();
  return crt_vtenc_real_pthread.cond_wait(cond, mutex);
}

#ifdef __cplusplus
}
#endif

#endif /* CRT_PORTING_SHIMS_MACOS_FFMPEG_VIDEOTOOLBOXENC_PTHREAD_DLFCN_COMPAT_H */
