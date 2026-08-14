#include <errno.h>
#include <sys/auxv.h>

#include <private/crt_auxv.h>

/* getauxval() -- see include/sys/auxv.h. Matches Android Bionic's
 * bionic/libc/bionic/getauxval.cpp: walk the kernel-provided auxiliary
 * vector until AT_NULL, return the matching entry's value, or 0 with
 * errno set to ENOENT if `type` never appears. */

static const struct crt_auxv_entry* auxv_entries(void) {
  char** p = __crt_initial_envp;

  if (p == 0) {
    return 0;
  }
  /* Standard Linux/System V process startup stack layout: argc, argv[],
   * NULL, envp[], NULL, auxv[], AT_NULL -- auxv begins immediately after
   * envp's own NULL terminator. __crt_initial_envp is set once, in
   * libc/src/env.c's __crt_env_set_initial(), directly from crt1.S's
   * untouched view of that same stack, never a copy, so this is safe to
   * walk regardless of anything __crt_env_init()/setenv()/putenv() have
   * done to the separate, mutable `environ` array since. */
  while (*p != 0) {
    ++p;
  }
  return (const struct crt_auxv_entry*)(p + 1);
}

unsigned long getauxval(unsigned long type) {
  const struct crt_auxv_entry* entry = auxv_entries();

  if (entry != 0) {
    for (; entry->a_type != AT_NULL; ++entry) {
      if (entry->a_type == type) {
        return entry->a_val;
      }
    }
  }
  errno = ENOENT;
  return 0;
}

/* Real glibc defines the actual implementation under this reserved-
 * namespace name and makes the public getauxval() a weak_alias to it --
 * *not* a Bionic convention (Bionic only ever exports plain getauxval()),
 * but real, load-bearing glibc ABI: LLVM compiler-rt's AArch64 outline-
 * atomics support (libclang_rt.builtins.a, __aarch64_have_lse_atomics'
 * own constructor, pulled in automatically by any code -- e.g. curl's use
 * of <stdatomic.h> -- whose atomic ops clang decides to outline rather
 * than inline) calls __getauxval() directly, by name, expecting exactly
 * this glibc-shaped symbol to exist, regardless of what this libc's own
 * public getauxval() happens to be implemented in terms of. Found for
 * real via linking curl: `undefined reference to '__getauxval'` traced
 * with `nm` straight to libclang_rt.builtins.a's lse-init.o, not to curl,
 * mbedtls, or zlib. Deliberately not declared in include/sys/auxv.h --
 * like real glibc, this is not part of the public API, only a linkable
 * symbol third-party runtime-support code is allowed to assume exists. */
unsigned long __getauxval(unsigned long type) {
  return getauxval(type);
}
