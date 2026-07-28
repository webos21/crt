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

## Remaining Gap

Detached Linux workers still cannot safely release their own stack mapping. The
kernel clears the child tid after the thread is exiting, and the worker is still
running on the stack that would need to be unmapped. For now, detached Linux
threads leave the stack/control storage to a future reaper or stack-cache
tranche.

The remaining lifecycle work is:

- add a minimal reaper or stack-cache policy for detached Linux workers;
- define the final thread-control block layout before wider TLS integration;
- decide whether to add architecture TLS setup with `CLONE_SETTLS`;
- define signal, cancellation, and robust mutex interaction with thread exit.

Until that work lands, joinable Linux threads use the intended futex-based
lifecycle, while detached Linux resource reclamation is still bootstrap-level.
