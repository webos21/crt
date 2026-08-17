# Bionic Libc Completeness Gaps Before `libcrtgfx`

## Goal

`docs/runtime_roadmap.md`'s "Order Of Work" says to reduce the remaining
planned libc/PAL work before starting the upper runtime in earnest, and its
own layering (`docs/project_meanings.md`'s "Architecture Implication")
places "ashmem/memfd-style shared memory" and other compatibility modules
*below* the graphics/application runtime layer, not something to improvise
mid-`libcrtgfx`. This is a real, evidence-based sweep (2026-08-16) of what's
actually present under `include/`/`libc/src/` versus real Android Bionic's
public surface, done before starting `libcrtgfx` (Skia + a Wayland-
compatible compositor boundary + Ozone, per `docs/runtime_roadmap.md`) --
not a guess from memory of what Bionic "probably" has. Every gap below was
confirmed by grepping this tree, not assumed.

This is a snapshot for prioritization, not a task list to execute
mechanically -- some gaps are cheap and worth doing now; others are properly
scoped for when the actual consuming work (the compositor boundary, `libuv`-
style event loop, etc.) begins.

## High priority: concretely blocks the Wayland-compositor-boundary goal

All four items originally listed here are now **done** -- see `HISTORY.md`'s
2026-08-16 entries.

- **`sendmsg`/`recvmsg` + `SCM_RIGHTS`/`CMSG_*` ancillary-data fd passing**
  -- **done**. `struct msghdr`/`struct cmsghdr`/`SCM_RIGHTS`/`CMSG_*`
  macros added to `include/sys/socket.h`; `sendmsg()`/`recvmsg()`
  implemented in `libc/src/socket.c` dispatching to new
  `__crt_sys_sendmsg()`/`__crt_sys_recvmsg()`. Linux/macOS get real raw
  syscall trampolines (`libc/src/arch/{linux,macos}/{x86_64,aarch64}/
  syscall.S`) with full native SCM_RIGHTS support -- **the syscall numbers
  were carefully reasoned from this project's own already-tested
  neighboring trampolines (e.g. Darwin's confirmed `recvfrom`=29/`accept`=
  30 anchoring `recvmsg`=27/`sendmsg`=28), matching the exact same
  reasoning-first pattern `linkat()`'s own trampolines used before real
  hardware testing closed that gap.** `tests/sendmsg_scm_rights_test.c`'s
  own real AF_UNIX fd-passing round trip has since run on real macOS
  hardware (2026-08-17) and closed this the same way: the syscall numbers
  themselves were correct, but it found and fixed four real ABI-
  translation bugs sitting between this project's Bionic-shaped structs
  and Darwin's real kernel ABI (AF_UNIX `sockaddr` translation, `struct
  msghdr` field widths, `CMSG_ALIGN`'s alignment unit, and `cmsg_level`/
  `SOL_SOCKET` translation) -- see `HISTORY.md`'s 2026-08-17 entry for the
  full writeup. Linux has not yet run this same real-hardware pass; its
  trampolines remain reasoned-not-independently-verified until it does.
  Windows has no `SCM_RIGHTS`-equivalent mechanism for `AF_UNIX` sockets at
  all (a fundamentally different, `DuplicateHandle()`-based, PID-targeted
  model) -- `__crt_sys_sendmsg()`/`__crt_sys_recvmsg()` there support
  plain multi-`iovec` data (gathered/scattered into one buffer, since
  Winsock has no native `sendmsg()`) but detect and reject an `SCM_RIGHTS`
  control message up front with `ENOTSUP`, failing loudly rather than
  silently dropping the fds a caller needed transferred.
- **`memfd_create`** -- **done**, but deliberately *not* as a Linux raw
  syscall (`include/sys/mman.h`, `libc/src/mman.c`). Implemented instead as
  a fully portable create-a-uniquely-named-file-then-unlink-it-immediately
  function -- the exact same proven technique this project's own
  `tmpfile()` already uses (see `libc/src/stdio.c`) -- rather than a
  per-host raw syscall/PAL feature. This was a deliberate choice over a
  real Linux `memfd_create` syscall trampoline specifically to avoid
  another *unverified* syscall number on top of the `sendmsg`/`recvmsg`
  ones above; the portable version is provably correct on every host right
  now (round-trip tested directly) and gives real Bionic-parity behavior
  for the thing that actually matters to the near-term consumer (an
  anonymous, `mmap(MAP_SHARED)`-able fd for `wl_shm`-style buffers) --
  just not Linux memfd's `F_ADD_SEALS`/`F_GET_SEALS` sealing support,
  which isn't implemented (`MFD_ALLOW_SEALING` is accepted but has no
  effect). `mmap` itself already supports real file-backed `MAP_SHARED`
  mappings on every host (confirmed in `libc/src/arch/windows/common/
  syscall.c`'s `__crt_sys_mmap`, using `CreateFileMappingA`/
  `MapViewOfFileEx`).
- **`semaphore.h`** -- **done** (2026-08-16). Was entirely absent, the most
  surprising gap given how complete the rest of the pthread story is; this
  project already had every primitive needed to implement it cheaply, and
  did: `sem_t`/`sem_init`/`sem_destroy`/`sem_wait`/`sem_trywait`/
  `sem_timedwait`/`sem_post`/`sem_getvalue` are implemented in
  `libc/src/semaphore.c` over the same private futex/wait-address layer
  (`__crt_wait32`/`__crt_wake32_*`) that already backs `pthread_mutex`/
  `pthread_cond`/`pthread_rwlock`. Named semaphores
  (`sem_open`/`sem_close`/`sem_unlink`) match real Bionic's own policy of
  declaring but never supporting them (`ENOSYS`). Regression:
  `tests/semaphore_test.c` (argument validation, a same-thread post/wait/
  trywait/getvalue round trip, a real `sem_timedwait` timeout, and a real
  cross-thread `pthread_create` + blocking `sem_wait()` + `sem_post()`
  wakeup, not just the CAS fast path).
- **Public `<stdatomic.h>`** -- **done** (2026-08-16). Was only a private
  internal layer over compiler `__atomic` builtins
  (`libc/include/private/crt_atomic.h`), limited to `int` atomics, a
  spinlock, and a once-state helper; see `docs/import_bionic.md`'s
  "Internal Atomic And Lock Tranche". Now a real public `include/
  stdatomic.h`, implemented over Clang's `__c11_atomic_*` builtins acting
  on real `_Atomic(T)`-qualified types (verified directly against this
  project's exact `-std=gnu99 -ffreestanding` build flags before writing
  it -- `_Generic` isn't available under `-std=gnu99`, a real C11-only
  feature, but turned out not to matter since `__c11_atomic_*` builtins
  are themselves already type-generic compiler magic, no `_Generic`
  dispatch needed). Covers `atomic_bool`/`atomic_int`/.../the `stdint.h`-
  backed atomic typedefs (`atomic_size_t`, `atomic_intptr_t`, etc.,
  `atomic_char16_t`/`atomic_char32_t` intentionally deferred alongside the
  still-missing `uchar.h`), `atomic_flag`, all the `atomic_*`/
  `atomic_*_explicit` operations, fences, and the `LOCK_FREE` macros.
  Regression: `tests/stdatomic_test.c`.

## Medium priority: commonly needed by graphics-adjacent native code

- **`sys/epoll.h`, `sys/eventfd.h`, `sys/timerfd.h`** -- **done**
  (2026-08-17), Linux-only, matching real Bionic exactly (Android's own
  Looper/ALooper implementation is built on exactly this; `wl_display`'s
  own recommended client integration pattern is "get the display's fd,
  epoll it alongside your other event sources," and this is also the
  standard shape for a `libuv`-style event loop, directly relevant to
  `libcrtjs`'s own "grow event loop... against the CRT/PAL" roadmap item).
  Declared on every host (`include/sys/{epoll,eventfd,timerfd}.h`) so
  portable code that merely includes and compiles against the surface
  keeps working everywhere -- matching this project's existing
  `libc/src/inotify.c` precedent for a Linux-only kernel feature -- but
  every function returns `ENOSYS` on macOS/Windows; no cross-platform
  abstraction pressure at the libc layer itself, matching real Bionic
  (macOS/Windows equivalents like `kqueue`/IOCP would be a higher-level
  PAL/event-loop design question for whichever layer actually needs
  portable multiplexed I/O, not a libc-parity concern).

  Linux gets real raw syscall trampolines (`libc/src/arch/linux/
  {x86_64,aarch64}/syscall.S`: `eventfd2`, `epoll_create1`/`epoll_ctl`/
  `epoll_pwait` -- `epoll_wait()` is implemented over `epoll_pwait` with a
  `NULL` sigmask, matching how glibc itself implements it, since aarch64
  has no separate `epoll_wait` syscall number at all -- and
  `timerfd_create`/`timerfd_settime`/`timerfd_gettime`), reasoned
  carefully from the same well-established, stable syscall tables
  `sendmsg`/`recvmsg`'s own trampolines were, but **not independently
  verified on real Linux hardware from this Windows-only session**,
  matching that exact same caveat. `struct epoll_event` needed particular
  care: the real Linux kernel ABI packs it to 12 bytes on x86_64
  (`__attribute__((packed))`, a historical ABI-compat quirk) but expects
  the naturally-aligned 16-byte layout on aarch64 -- an architecture-
  conditional version of the same class of bug `struct cmsghdr`'s
  Linux-vs-macOS `cmsg_len` width mismatch was (see the `sendmsg`/
  `recvmsg` entry above and `HISTORY.md`'s 2026-08-16 cmsghdr-fix entry).
  A compile-time size check in `include/sys/epoll.h` (`sizeof(struct
  epoll_event) == 12` on x86_64, `== 16` elsewhere) exists specifically to
  catch a mistake here before it becomes a silent runtime data corruption,
  and runs on every host/architecture this project builds for, not just
  Linux. New regressions: `tests/eventfd_test.c` (real accumulate/drain
  round trip on Linux, `ENOSYS` check elsewhere), `tests/timerfd_test.c`
  (real one-shot-timer-fires-and-is-reported-via-poll round trip on
  Linux, `ENOSYS` check elsewhere), `tests/epoll_test.c` (the
  architecture-conditional size check on every host, plus a real
  add-a-pipe-fd/observe-it-become-readable/remove-it round trip on
  Linux).
- **`dl_iterate_phdr`/`link.h`, `elf.h`, `dladdr`** -- absent (`dlfcn.h`
  only has `dlopen`/`dlsym`/`dlclose`/`dlerror`, confirmed by reading the
  file directly). Real Bionic has all of these. Used by some GPU driver
  loaders (Mesa's own ICD/driver enumeration walks loaded ELF images in
  some paths) and by unwind/crash-handling code.
- **`PTHREAD_PROCESS_SHARED`** -- the attribute constant is exposed, but
  `pthread_mutex`/`pthread_rwlock`/`pthread_spinlock` all explicitly return
  `ENOTSUP` for it today (confirmed in `docs/import_bionic.md`'s own
  tranche notes). Relevant for real cross-process shared-memory
  synchronization in a compositor architecture, though many compositor
  protocols (including Wayland) route around needing this by using
  message-passing instead of shared locks -- lower urgency than the
  fd-passing items above.

## Lower priority: general POSIX/Bionic completeness, no identified near-term consumer yet

Real Bionic has all of these; this project doesn't yet, and nothing in the
current roadmap concretely needs them:

- `glob.h` (`glob`/`globfree`)
- `sys/prctl.h` (`prctl` -- Android uses this heavily for thread naming,
  seccomp, etc., but this project already has its own thread-naming path)
- `ucontext.h` (`getcontext`/`setcontext`/`makecontext`/`swapcontext`)
- `ifaddrs.h` (`getifaddrs`/`freeifaddrs`)
- `threads.h` (C11 `<threads.h>`, a thin wrapper over pthreads in real
  Bionic)
- `uchar.h` (`char16_t`/`char32_t`, `c16rtomb`/`c32rtomb` etc.)

`glob.h`/`wordexp.h`/`nl_types.h`/`aio.h` are lower priority still --
real Bionic either doesn't implement them meaningfully (`wordexp`/`aio_*`
are effectively stubs even on Android) or they have no plausible graphics-
stack consumer.

Already known and tracked elsewhere, not new findings, listed here only for
completeness against this same sweep:

- **C++ exceptions/RTTI across the runtime boundary** -- `docs/
  cxx_runtime.md` already documents this as deferred (`-fno-exceptions
  -fno-rtti` required for now). Relevant to Skia/V8 eventually, but a
  known, separately-scoped, larger effort, not a new finding from this
  sweep.
- **`pthread_cancel`** -- declared, but a real `ENOTSUP` stub
  (`libc/src/pthread.c`), matching `docs/import_bionic.md`'s "Cancellation
  and robust mutexes are still deferred."
