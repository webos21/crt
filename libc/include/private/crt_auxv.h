#ifndef CRT_PRIVATE_CRT_AUXV_H
#define CRT_PRIVATE_CRT_AUXV_H

/* One ELF auxiliary-vector entry. Matches Elf64_auxv_t's layout on every
 * LP64 target this project builds for (aarch64/x86_64 Linux): two
 * pointer-sized fields, a_type and a_un.a_val (flattened here since this
 * project has no 32-bit target that would need the union-of-differently-
 * sized-members Elf32_auxv_t shape upstream keeps this a union for). See
 * include/sys/auxv.h / libc/src/arch/linux/common/auxv.c. */
struct crt_auxv_entry {
  unsigned long a_type;
  unsigned long a_val;
};

/* Set once, in libc/src/env.c's __crt_env_set_initial(), to the untouched,
 * kernel-provided initial envp pointer (never a copy -- see that file).
 * Linux's ELF auxiliary vector (see include/sys/auxv.h / libc/src/arch/
 * linux/common/auxv.c) is read out of this same pointer rather than
 * needing any crt1.S changes: the standard Linux/System V process startup
 * stack layout is `argc, argv[], NULL, envp[], NULL, auxv[], AT_NULL`, so
 * auxv begins immediately after envp's own NULL terminator. Portable
 * (declared/set here regardless of target OS, matching __crt_env_set_
 * initial() itself), but only ever read by Linux-specific code -- on
 * macOS/Windows nothing walks past the envp NULL terminator, since neither
 * host's real process startup layout has a Linux-shaped auxv there. */
extern char** __crt_initial_envp;

#endif
