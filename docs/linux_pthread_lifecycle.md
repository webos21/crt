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
waits for that word to become zero through the private futex wait primitive,
which matches the normal child-tid-clearing shape used by Linux pthread
implementations more closely than the earlier `wait4` bootstrap join.

## Detached Reaper

Detached Linux workers cannot safely release their own stack mapping because the
worker is still executing on that stack while the kernel clears the child tid.
The runtime therefore starts one permanent Linux reaper thread on demand. A
detached worker queues its project control block before thread exit, and the
reaper waits on the queued tid word until the kernel clears it. Once the tid word
is zero, the reaper releases the owned stack mapping and the control block.

Caller-provided stacks are not unmapped by the reaper; ownership remains with the
caller that supplied the stack through `pthread_attr_setstack`.

## Remaining Work

The remaining lifecycle work is:

- define the final thread-control block layout before wider TLS integration;
- decide whether to add architecture TLS setup with `CLONE_SETTLS`;
- define signal, cancellation, and robust mutex interaction with thread exit;
- decide whether the permanent reaper should eventually be replaced by a stack
  cache for high-volume detached-thread workloads.

Joinable Linux threads now use the intended futex-based lifecycle, and detached
Linux threads have project-owned stack/control reclamation through the reaper.
