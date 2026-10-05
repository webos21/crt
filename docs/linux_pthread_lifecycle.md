# Linux Pthread Lifecycle Notes

This note records the current Linux pthread backend and the remaining lifecycle
work.

## Current State

The Linux backend now creates project pthreads with a raw `clone` wrapper using:

- `CLONE_VM`
- `CLONE_FS`
- `CLONE_FILES`
- `CLONE_SIGHAND`
- `CLONE_THREAD`
- `CLONE_SYSVSEM`
- `CLONE_PARENT_SETTID`
- `CLONE_CHILD_SETTID`
- `CLONE_CHILD_CLEARTID`

The project control block contains a kernel-visible tid word. `pthread_join`
waits for that word to become zero through a raw futex wait, which matches the
kernel wake issued for `CLONE_CHILD_CLEARTID` more closely than the earlier
`wait4` bootstrap join. The internal wait helpers still use private futexes for
runtime-owned synchronization words such as mutexes and condition variables.

Current-thread identity, `errno`, pthread key values, and thread names are now
routed through the private `crt_tls` adapter. The Linux adapter intentionally
uses a kernel-tid keyed runtime registry for project-created clone threads. This
keeps `pthread_self()` and `__errno()` on the same thread-context abstraction
without forcing macOS or Windows to emulate Linux TLS internals.

**Native ELF TLS (2026-10-04, x86_64; aarch64 added 2026-10-05).** Compiler-level `thread_local`/`__thread`
used to be shared by every CRT thread, because `clone` ran without `CLONE_SETTLS`
and the child inherited the creator's thread pointer: one address, values bleeding
between threads (reproduced with a two-thread counter; found when JavaScriptCore
failed in any thread but the first). `libc/src/arch/linux/common/thread_tls.c`
now builds a thread control block, dtv and static TLS block for each new thread
from public structure only -- the SVR4 `r_debug`/`link_map` list, `PT_TLS` program
headers, and the TLS ABI's variant II rules -- reading each startup module's exact
thread-pointer offset from the initial thread's dtv, initialising every block from
the module's own `PT_TLS` image (never from the initial thread's modified copy),
copying the control block header, and re-aiming the tcb/self/dtv pointers. The
block lives at the top of the thread's stack mapping and is freed with it;
`pthread_create` passes the new thread pointer with `CLONE_SETTLS`. It declines
(the old behaviour) when the layout is not exactly as expected. Limits: the glibc
dynamic loader only, and modules loaded later with `dlopen` have no static block
to clone (CRT's `dlopen` is a stub).

*aarch64 (variant I).* The thread pointer is `tpidr_el0` and addresses a 16-byte
header (`dtv`, `private`); the static blocks follow it at positive offsets and the
loader's thread descriptor (glibc's `struct pthread`, 0x720 bytes on glibc 2.43)
lies below it. Block offsets are read from the initial thread's dtv exactly as on
x86_64, and the new thread pointer is aligned to the strictest `p_align`. The
descriptor's size is a loader internal that `ld.so` does not export, so the window
below the thread pointer (at most 0x1000 bytes) is copied at the same
thread-pointer-relative offsets the loader's code uses (it reads `tp-40`, `tp-0x720`,
`tp-0x71c` and the `tid` field) and is clamped to pages that `mincore()` proves are
mapped, so nothing unmapped is ever read; setup declines when fewer than 0x100 bytes
are mapped. `__crt_sys_clone_thread` already passed the TLS argument in the aarch64
`clone` order. Verified with `pthread_native_tls_test` (and its decline mutation) and
with JavaScriptCore on 4 and 8 threads, which exercises three shared-library TLS
modules (`libJavaScriptCore`, `libc++abi`, `libicuuc`).
`pthread_native_tls_test` covers it; `pthread_getattr_np` now describes the real
stack of the initial thread.

## Detached Reaper

Detached Linux workers cannot safely release their own stack mapping because the
worker is still executing on that stack while the kernel clears the child tid.
The runtime therefore starts one permanent Linux reaper thread on demand. A
detached worker queues its project control block before thread exit, and the
reaper waits on the queued tid word until the kernel clears it. Once the tid word
is zero, the reaper releases the owned stack mapping and the control block.

Caller-provided stacks are not unmapped by the reaper; ownership remains with the
caller that supplied the stack through `pthread_attr_setstack`.

The process-level Linux exit path uses `exit_group`, while pthread worker exit
uses the per-thread `exit` syscall. This distinction matters once permanent
runtime helper threads such as the detached reaper exist; returning from `main`
must terminate the whole process, not only the initial thread.

## Remaining Work

The remaining lifecycle work is:

- native TLS exists for x86_64 and aarch64 (above); decide how the eventual
  CRT-owned linker's TLS module table replaces the
  loader-layout cloning; then move the `crt_tls` registry's current-thread lookup
  (a `gettid` system call per access) onto the thread pointer;
- define signal, cancellation, and robust mutex interaction with thread exit;
- decide whether the permanent reaper should eventually be replaced by a stack
  cache for high-volume detached-thread workloads.

Joinable Linux threads now use the intended futex-based lifecycle, and detached
Linux threads have project-owned stack/control reclamation through the reaper.
