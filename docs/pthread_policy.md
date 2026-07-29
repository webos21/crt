# pthread policy

This project keeps a Bionic-shaped public pthread ABI while adapting the backend
to Linux, macOS, and Windows.

## Implemented baseline

- Thread create, join, detach, exit, and `pthread_self`.
- `pthread_once`.
- Thread-specific data keys and four-pass TLS destructor execution.
- Mutexes: normal, recursive, and error-check.
- Condition variables backed by the project wait-by-address layer.
- Read/write locks.
- Spin locks.
- Barriers.
- Attribute objects for detach state, stack size, user stack, guard size, basic
  scheduler fields, process-shared policy, and robust-mutex policy.
- Bionic extension surface for `pthread_getattr_np`, `pthread_gettid_np`,
  `pthread_setname_np`, `pthread_getname_np`, `pthread_getcpuclockid`, and
  `pthread_setschedprio`.

## Backend policy

- Linux uses project-owned `clone` threads and a detached-thread reaper. Joinable
  threads release their control block and owned stack during `pthread_join`.
- macOS keeps the project pthread ABI and calls libSystem pthread entry points
  behind the adaptation layer for native thread lifecycle.
- Windows keeps the project pthread ABI and maps thread lifecycle to Kernel32
  thread and TLS primitives.
- Project-created threads return the project control-block handle from
  `pthread_self`, so it matches the `pthread_t` value returned by
  `pthread_create`. Threads not created by this runtime fall back to the backend
  thread id for `pthread_self`.

## Scheduler and stack policy

- Scheduler attribute constants follow Bionic: `PTHREAD_EXPLICIT_SCHED` is `0`
  and `PTHREAD_INHERIT_SCHED` is `1`.
- `pthread_attr_setschedpolicy` and `pthread_attr_setschedparam` store
  `SCHED_OTHER`, `SCHED_FIFO`, `SCHED_RR`, and the requested priority in the
  attribute object, matching Bionic's attr-object behavior.
- `pthread_create` does not apply stored scheduler attributes to the host
  backend yet. Linux/Bionic uses `sched_setscheduler` at creation time, but this
  runtime defers that cross-platform mapping until a later backend tranche.
- Direct thread scheduler changes through `pthread_setschedparam` are limited
  to the no-op `SCHED_OTHER` with priority `0`. Other policies or non-zero
  priority return `ENOTSUP`.
- Process scope returns `ENOTSUP`.
- Linux-owned thread stacks apply the recorded guard size with `mprotect` before
  the usable stack region. The default guard size is one 4096-byte page.
- `pthread_attr_setstack` is supported at the attribute-object level on every
  host. `pthread_attr_getstack` returns the caller-provided address and size even
  on hosts that cannot apply that stack during thread creation.
- Caller-provided stacks disable runtime guard ownership, matching the rule that
  stack ownership remains with the caller.
- macOS passes stack size, guard size, and caller-provided stacks into native
  libSystem pthread attributes when the native entry points are available.
- Windows passes stack size to `CreateThread`. Caller-provided stacks return
  `ENOTSUP` at `pthread_create` time, not at `pthread_attr_setstack` time,
  because Kernel32 does not accept an arbitrary caller-owned stack for
  `CreateThread`.

## Bionic extension policy

- `pthread_getattr_np` returns the stored project pthread attributes. For the
  main thread, it returns default attributes.
- `pthread_gettid_np` returns the backend thread id when the project has a
  control block. For the current thread it asks the backend directly.
- Thread names are stored in the project control block or current-thread TLS.
  Backend-visible OS thread naming is deferred.
- `pthread_getcpuclockid` is present but returns `ENOTSUP`; per-thread CPU clock
  mapping is deferred.

## Explicitly unsupported

- Process-shared pthread objects return `ENOTSUP`.
- Robust mutexes return `ENOTSUP`; the default robust policy is
  `PTHREAD_MUTEX_STALLED`.
- Thread cancellation is not implemented. `pthread_cancel` returns `ENOTSUP`.
  Cancellation state/type setters accept the disabled/deferred no-op state, and
  enabling or asynchronous cancellation returns `ENOTSUP`.

These choices are intentional bootstrap constraints, not accidental gaps. They
keep the ABI surface linkable while preventing callers from assuming semantics
that the current runtime cannot honor.
