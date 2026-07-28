# Linux Pthread Lifecycle Plan

This note records the next Linux pthread backend direction after the first
portable pthread tranche.

## Current State

The Linux backend currently creates a thread-like task with a project-owned
stack and a raw `clone` wrapper, then joins it with `wait4`. This is deliberately
simple and keeps early bring-up debuggable across Linux, Windows, and macOS.

This model is good enough for current tests, but it is not the final pthread
lifecycle model:

- join is process-child oriented rather than `CLONE_THREAD` oriented;
- detached thread stack reclamation is deferred;
- thread group semantics, child-tid clearing, and futex-backed join are not yet
  represented;
- signal, cancellation, robust mutex, and destructor ordering relative to
  kernel-visible thread teardown need a clearer lifecycle boundary before
  broader library porting.

## Target Direction

The next Linux pthread lifecycle backend should move toward the normal NPTL /
Bionic-shaped model:

- create workers with `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
  CLONE_THREAD | CLONE_SYSVSEM`;
- use `CLONE_CHILD_CLEARTID` and `CLONE_CHILD_SETTID` once the project has a
  stable thread-control block location for the kernel-visible tid word;
- implement join as a wait on that tid word through the private futex primitive;
- let detached threads release project control storage at thread exit;
- add a minimal reaper or stack-cache policy so detached Linux stacks are not
  leaked;
- run pthread key destructors before the tid word is cleared and before detached
  storage is released.

## Proposed Implementation Order

1. Split `crt_pthread_control` into portable state and Linux-private state.
2. Add a Linux tid word that can be passed as child tid to `clone`.
3. Add architecture syscall wrappers for the final `clone` argument set.
4. Switch join from `wait4` to `__crt_wait32` on the tid word.
5. Add detached stack reclamation policy.
6. Extend tests for explicit `pthread_exit`, join result, detached completion,
   and destructor execution on Linux.

Until this work lands, the current Linux lifecycle should be treated as a
bootstrap compatibility layer, not a final pthread ABI contract.
