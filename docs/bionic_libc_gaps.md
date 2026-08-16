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

- **`sendmsg`/`recvmsg` + `SCM_RIGHTS`/`CMSG_*` ancillary-data fd passing**
  -- entirely absent (`include/sys/socket.h` has `socket`/`bind`/`connect`/
  `send`/`recv`/`sendto`/`recvfrom` but no `struct msghdr`/`struct
  cmsghdr`/`sendmsg`/`recvmsg` at all). This is Wayland's core wire-protocol
  mechanism: every `wl_shm` buffer, DMA-BUF, and even the initial socket
  handshake passes fds between client and compositor over a Unix domain
  socket using exactly this mechanism. Nothing resembling a Wayland
  compositor boundary is possible without it. Real Bionic has the full
  surface. Windows has no native `AF_UNIX` + `SCM_RIGHTS` equivalent --
  expect this to need real PAL design work (Windows named-pipe/handle-
  duplication tricks or an explicit "not supported on Windows" boundary),
  not a thin syscall wrapper the way Linux/macOS get.
- **`memfd_create`** -- absent. The standard modern mechanism for creating
  an anonymous, shared-memory-backed fd to `mmap(MAP_SHARED)` and hand to
  another process via the `SCM_RIGHTS` mechanism above -- what real `wl_shm`
  clients use today instead of the older, Bionic-unsupported POSIX
  `shm_open`. Real Bionic has it as a thin Linux syscall wrapper. `mmap`
  itself already supports real file-backed `MAP_SHARED` mappings on every
  host (confirmed in `libc/src/arch/windows/common/syscall.c`'s
  `__crt_sys_mmap`, which uses `CreateFileMappingA`/`MapViewOfFileEx` for
  the file-backed path) -- only the "get an anonymous shareable fd in the
  first place" piece is missing.
- **`semaphore.h`** (`sem_t`, `sem_init`, `sem_destroy`, `sem_wait`,
  `sem_trywait`, `sem_timedwait`, `sem_post`, `sem_getvalue`) -- entirely
  absent, the most surprising gap given how complete the rest of the
  pthread story is. This is a baseline POSIX threading primitive nearly
  every threaded C/C++ library expects (Skia's own thread pool, FFmpeg,
  the Vulkan loader, etc.), and this project already has every primitive
  needed to implement it cheaply: the private futex/wait-address layer
  (`__crt_wait32`/`__crt_wake32_*`, see `docs/import_bionic.md`'s "Private
  Wait/Futex Tranche") already backs `pthread_mutex`/`pthread_cond`/
  `pthread_rwlock` the same way a semaphore's counter+wait would need.
  Real Bionic has the full POSIX unnamed-semaphore surface (named
  semaphores -- `sem_open`/`sem_close`/`sem_unlink` -- are Bionic stubs
  returning `ENOSYS`, so parity there is trivial too).
- **Public `<stdatomic.h>`** -- currently only a private internal layer
  over compiler `__atomic` builtins (`libc/include/private/crt_atomic.h`),
  limited to `int` atomics, a spinlock, and a once-state helper; see
  `docs/import_bionic.md`'s "Internal Atomic And Lock Tranche" ("This is
  not a public C11 `<stdatomic.h>` import... deferred"). Real Bionic
  exposes a real C11 `<stdatomic.h>` (a thin wrapper over the same kind of
  compiler builtins this project already uses internally). QuickJS, V8,
  and Skia's own threading code all commonly include `<stdatomic.h>`
  directly as a hard dependency -- this was already a known, tracked gap,
  resurfaced here because it now has a concrete, near-term consumer
  (`libcrtjs`'s QuickJS bring-up is roadmap step 3, immediately before
  Skia) rather than being purely hypothetical.

## Medium priority: commonly needed by graphics-adjacent native code

- **`sys/epoll.h`, `sys/eventfd.h`, `sys/timerfd.h`** -- all three absent;
  real Bionic has all three (Linux-only, matching Android's own Looper/
  ALooper implementation, which is built on exactly this). `wl_display`'s
  own recommended client integration pattern is "get the display's fd,
  epoll it alongside your other event sources" -- and this is also the
  standard shape for a `libuv`-style event loop, directly relevant to
  `libcrtjs`'s own "grow event loop... against the CRT/PAL" roadmap item.
  Linux-only in real Bionic too, so no cross-platform abstraction pressure
  at the libc layer itself -- macOS/Windows equivalents (`kqueue`/IOCP)
  aren't a Bionic-parity concern, they'd be a higher-level PAL/event-loop
  design question for whichever layer actually needs portable multiplexed
  I/O.
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
