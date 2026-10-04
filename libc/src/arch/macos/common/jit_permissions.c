/* Executable-memory permissions on Apple Silicon (Web Tranche 1C macOS replay).
 *
 * macOS arm64 refuses memory that is writable and executable at once unless it was mapped with
 * MAP_JIT, and for such memory every thread chooses, with pthread_jit_write_protect_np(), whether
 * the region is read-write or read-execute *for that thread*. libc/src/mman.c maps an anonymous
 * PROT_WRITE|PROT_EXEC request with MAP_JIT (so code written for Linux's plain RWX mapping gets a
 * mapping the kernel allows); these two functions are the per-thread toggle, in the shape of
 * JavaScriptCore's OS_THREAD_SELF_RESTRICT extension point (JavaScriptCore/assembler/
 * FastJITPermissions.h): "supported" says whether this host needs/has the toggle at all and
 * __crt_jit_write_protect(0) makes the calling thread's JIT regions writable, (1) executable.
 *
 * The two Apple functions are the host OS boundary (libsystem_pthread); they have no Bionic
 * counterpart and are deliberately not part of the public headers. x86_64 macOS allows plain RWX, so
 * the toggle is unsupported there. */

#if defined(__aarch64__)
extern int pthread_jit_write_protect_supported_np(void);
extern void pthread_jit_write_protect_np(int enabled);

int __crt_jit_write_protect_supported(void) {
  return pthread_jit_write_protect_supported_np() != 0;
}

void __crt_jit_write_protect(int enabled) {
  pthread_jit_write_protect_np(enabled != 0);
}
#else
int __crt_jit_write_protect_supported(void) {
  return 0;
}

void __crt_jit_write_protect(int enabled) {
  (void)enabled;
}
#endif
