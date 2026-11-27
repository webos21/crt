#include <errno.h>

#include <private/crt_macho_symbol.h>

/* The fork() behind this CRT's public fork() on macOS is Apple's own, not the raw system call.
 *
 * macOS threads here are real Apple pthreads (libc/src/pthread.c), and libSystem keeps per-process
 * state that is wrong in a copy of the process: cached Mach ports above all, but also libpthread,
 * libmalloc, dyld and libdispatch state. Only libsystem_c's fork() runs the hooks that reset it
 * (libSystem's atfork child handlers). With a bare svc, the child's first pthread_create() died:
 * mach_vm_allocate() used a stale port cache, mig_get_reply_port() and
 * _kernelrpc_mach_port_construct() recursed into each other, and the stack guard raised SIGSEGV.
 *
 * Bionic's _Fork() likewise leaves its libc usable in the child and differs from fork() only in
 * skipping pthread_atfork() handlers. This CRT's own handler list is separate from libSystem's,
 * so calling this from fork() runs each CRT handler exactly once. Handlers that host libraries
 * registered with Apple's pthread_atfork() run too, as they expect to.
 *
 * Looked up by exact dylib path: "fork" would otherwise bind to this CRT's own definition. */

extern int __crt_macos_to_bionic_errno(int error);

long __crt_macos_libsystem_fork(void) {
  int (*fork_fn)(void) = (int (*)(void))__crt_macho_find_symbol_in_loaded_image(
      "/usr/lib/system/libsystem_c.dylib", "fork");
  int* (*error_fn)(void) = (int* (*)(void))__crt_macho_find_symbol_in_loaded_image(
      "/usr/lib/system/libsystem_kernel.dylib", "__error");
  int child;

  if (fork_fn == 0 || error_fn == 0) {
    return -ENOSYS;
  }
  child = fork_fn();
  if (child < 0) {
    return -__crt_macos_to_bionic_errno(*error_fn());
  }
  return child;
}
