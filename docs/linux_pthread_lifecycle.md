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
uses a kernel-tid keyed runtime registry for project-created clone threads until
the backend grows a Bionic-style TCB/TLS setup with real thread-pointer
initialization. This keeps `pthread_self()` and `__errno()` on the same
thread-context abstraction without forcing macOS or Windows to emulate Linux
TLS internals.

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

- replace the temporary Linux `crt_tls` registry with an architecture-backed
  Bionic-style TCB/TLS setup;
- decide how `CLONE_SETTLS`, ELF TLS, and the eventual linker TLS module table
  connect to the project thread context;
- define signal, cancellation, and robust mutex interaction with thread exit;
- decide whether the permanent reaper should eventually be replaced by a stack
  cache for high-volume detached-thread workloads.

Joinable Linux threads now use the intended futex-based lifecycle, and detached
Linux threads have project-owned stack/control reclamation through the reaper.
