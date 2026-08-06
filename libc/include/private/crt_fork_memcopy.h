#ifndef CRT_PRIVATE_CRT_FORK_MEMCOPY_H
#define CRT_PRIVATE_CRT_FORK_MEMCOPY_H

/* Windows aarch64 Cygwin/MSYS-style fork() replacement
 * (docs/windows_fork_emulation.md, "Spawn Broker Retired"): spawns a
 * CREATE_SUSPENDED clone of the current executable under the mitigation
 * policy verified to make heap/stack addresses deterministic, copies the
 * calling thread's live state (heap chunks, stack, image .data/.bss, TLS
 * context block) into it via WriteProcessMemory(), then redirects its
 * initial thread straight into a trampoline that longjmp()s a copied
 * jmp_buf to resume at the original fork() call site.
 *
 * Return value mirrors __crt_sys_fork()'s own contract:
 *   0            -- this is the child, resumed via longjmp; out params
 *                    are not touched.
 *   1            -- this is the parent; out_child_pid and out_child_process
 *                    are filled in. Caller (syscall.c's __crt_sys_fork())
 *                    is responsible for child-process bookkeeping
 *                    (remember_child_process()).
 *   negative     -- -errno; no child was created. */
long __crt_windows_memcopy_fork(unsigned long* out_child_pid, void** out_child_process);

#endif
