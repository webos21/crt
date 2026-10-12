# Bionic Libc Completeness Gaps Before `libcrtgfx`

> **Status:** This is the completed pre-`libcrtgfx` audit ledger, not the
> current work queue. Every high/medium/lower-priority item identified by this
> sweep was subsequently implemented; later real-host corrections and
> verification are recorded in `HISTORY.md`. Current limitations live in
> `STATUS.md`/`TODO.md`. Caveats below are preserved as evidence of what was
> known at the time each tranche landed.

## Goal

`docs/design/runtime_roadmap.md`'s "Order Of Work" says to reduce the remaining
planned libc/PAL work before starting the upper runtime in earnest, and its
own layering (`docs/design/project_meanings.md`'s "Architecture Implication")
places "ashmem/memfd-style shared memory" and other compatibility modules
*below* the graphics/application runtime layer, not something to improvise
mid-`libcrtgfx`. This is a real, evidence-based sweep (2026-08-16) of what's
actually present under `include/`/`libc/src/` versus real Android Bionic's
public surface, done before starting `libcrtgfx` (Skia + a Wayland-
compatible compositor boundary + Ozone, per `docs/design/runtime_roadmap.md`) --
not a guess from memory of what Bionic "probably" has. Every gap below was
confirmed by grepping this tree, not assumed.

This is a snapshot for prioritization, not a task list to execute
mechanically -- some gaps are cheap and worth doing now; others are properly
scoped for when the actual consuming work (the compositor boundary, `libuv`-
style event loop, etc.) begins.

## High priority: concretely blocks the Wayland-compositor-boundary goal

All four items originally listed here are now **done** -- see [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s
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
  `SOL_SOCKET` translation) -- see [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s 2026-08-17 entry for the
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
  spinlock, and a once-state helper; see `docs/guides/import_bionic.md`'s
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
  any future upper-runtime event loop, e.g. `libcrtweb`'s).
  Declared on every host (`include/sys/{epoll,eventfd,timerfd}.h`) so
  portable code that merely includes and compiles against the surface
  keeps working everywhere -- matching this project's existing
  `libc/src/inotify.c` precedent for a Linux-only kernel feature -- but
  every function returns `ENOSYS` on macOS/Windows; no cross-platform
  abstraction pressure at the libc layer itself, matching real Bionic
  (macOS/Windows equivalents like `kqueue`/IOCP would be a higher-level
  PAL/event-loop design question for whichever layer actually needs
  portable multiplexed I/O, not a libc-parity concern).

  **Update 2026-09-01: `eventfd()` gained a real (not stubbed) Windows
  implementation.** A portable consumer's own configure-time feature
  probe (curl's) link-detected the ENOSYS-stub `eventfd()`/`<sys/
  eventfd.h>` above as usable and broke as a result (see `HISTORY.md`'s
  dated entry and `porting/recipes/curl.json`'s own notes for the full
  regression writeup) -- rather than route around it again, `eventfd()`
  was actually implemented for real on Windows: a manual-reset Win32
  Event `HANDLE` as the wait/wake primitive plus a 64-bit counter
  (`CRT_FD_KIND_EVENTFD` in `libc/src/arch/windows/common/syscall.c`),
  matching real Linux `eventfd2(2)` semantics -- accumulate-on-write,
  drain-or-decrement-on-read (`EFD_SEMAPHORE`), `EFD_NONBLOCK`/
  `EFD_CLOEXEC` mapped onto this project's existing `fcntl()` machinery,
  and real `poll()`/`select()` integration. `epoll`/`timerfd` remain
  `ENOSYS`-stubbed on Windows for now (no consumer has hit the same
  false-positive-detection problem for either of those yet) -- same
  future-work shape as `eventfd()` had before this update, should one
  come up. `tests/eventfd_test.c` now runs its real-behavior branch (not
  just the `ENOSYS` check) on Windows too, extended with `EFD_SEMAPHORE`
  and a genuine cross-thread blocking-read/wake coverage case.

  **Update 2026-09-02: `eventfd()` gained a real (not stubbed)
  macOS implementation too**, for the same reason as Windows's above --
  investigated on real macOS hardware while getting curl's own
  `--disable-threaded-resolver` configure workaround off macOS (see
  `HISTORY.md`'s dated entry and `porting/recipes/curl.json`'s own
  notes). Unlike Windows (no POSIX fd/poll-compatible kernel object to
  repurpose, hence the from-scratch Win32 Event `HANDLE` design above),
  macOS already has one: a real `pipe(2)` pair, which is natively
  `poll()`/`select()`/`kqueue`-compatible and cross-thread-signalable on
  its own. The emulation is a software counter/semaphore layer on top
  of that pipe, with one invariant driving it -- exactly one byte sits
  in the pipe whenever the counter is nonzero, drained the moment it
  returns to zero -- so real readiness/blocking-wait never needs
  reimplementing (`libc/src/fd.c`'s own "Real eventfd() emulation for
  macOS" comment has the full design, including the double-consumption
  hazard a blocking reader and the "drain to zero" path both racing for
  the same physical byte, and how re-poking the pipe after a
  `EFD_SEMAPHORE` blocking read restores the invariant for other
  waiters). `epoll`/`timerfd` remain `ENOSYS`-stubbed on macOS for now,
  same future-work shape as before. `tests/eventfd_test.c` runs its
  real-behavior branch on macOS too now, run 20 times back-to-back
  specifically checking the cross-thread blocking-wakeup case for
  flakiness (none found).

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
  `recvmsg` entry above and [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s 2026-08-16 cmsghdr-fix entry).
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
- **`dl_iterate_phdr`/`link.h`, `elf.h`, `dladdr`** -- **done** (2026-08-17).
  `include/elf.h` (ELF64 types/constants -- a fixed, documented System V
  ABI binary spec, not host-dependent the way syscall numbers are, so no
  unverified-hardware caveat applies to it); `include/link.h` (`struct
  dl_phdr_info`, matching real Bionic's own minimal 4-field shape, not
  glibc's larger extension); `dladdr`/`Dl_info` added to `include/dlfcn.h`.
  Real per-host implementations, not stubs, wherever each host actually
  has something real to report:
  - **Linux** (`libdl/src/arch/linux/dl_linux.c`): `dl_iterate_phdr()`
    reports exactly one entry -- the main executable -- built from the
    real `AT_PHDR`/`AT_PHNUM` values the kernel handed this process at
    `exec()` (via the existing `getauxval()`). Only one entry because this
    project has no real ELF dynamic linker yet (`docs/design/dynamic_loading.md`:
    Linux `dlopen()` doesn't actually load shared objects today), not a
    limitation of this feature specifically. The load bias is computed
    from the `PT_PHDR` segment's own link-time `p_vaddr` when present
    (falls back to `0`, correct for a non-PIE executable). `dladdr()`
    checks whether the address falls inside one of that same executable's
    `PT_LOAD` segments and, if so, reports its real path via
    `/proc/self/exe`.
  - **macOS** (`libdl/src/arch/macos/dl_macos.c`): `dl_iterate_phdr()`
    calls the callback zero times and returns `0` -- `dlpi_phdr`/
    `dlpi_phnum` are fundamentally `Elf64_Phdr`-shaped, and Mach-O has no
    such structure at all (real load commands/segment commands are a
    different format); fabricating ELF-shaped data from real Mach-O data
    would be actively wrong for any caller walking the array expecting
    real ELF semantics, not just imprecise, so this is an honest "no ELF
    images to report" rather than a stub. `dladdr()` is real: a new shared
    helper (`__crt_macho_find_image_for_address()`, added to
    `libc/src/arch/macos/common/macho_symbol.c` and exposed via
    `crt_macho_symbol.h` since it needed the same dyld-image-walking
    infrastructure `dlopen()`/`dlsym()` already use) walks every loaded
    image's `LC_SEGMENT_64` commands for the one whose real, slide-
    adjusted address range contains the target address.
  - **Windows** (`libdl/src/arch/windows/dl_windows.c`): `dl_iterate_phdr()`
    is the same honest zero-entries result as macOS, for the same reason
    (PE has no ELF program headers either). `dladdr()` is real:
    `GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)` finds
    which loaded module contains the address directly (no manual PE
    parsing needed) -- an `HMODULE`'s own value is documented to equal the
    module's real load base address on Windows, so it doubles directly as
    `dli_fbase`.

  On every host, `dli_sname`/`dli_saddr` (the nearest-symbol part of the
  real `dladdr()` contract) are always left `NULL`/`0` -- POSIX/Bionic both
  document that as a legitimate result when no matching symbol is found,
  not a failure, and this project does not parse any host's symbol table
  for reverse address-to-name lookup yet. New regression: `tests/
  dl_iterate_phdr_dladdr_test.c` (real `elf.h` struct-size checks on every
  host; `dl_iterate_phdr()` internal-consistency checks tolerant of either
  the zero-entries or one-entry shape; a real `dladdr()` lookup against
  the test binary's own `main()`, verified directly on Windows -- Linux/
  macOS are reasoned carefully but not yet run on real hardware from this
  session, matching this same pattern's other entries above).
- **`PTHREAD_PROCESS_SHARED`** -- done (2026-08-17). Real, differentiated
  per-primitive support:
  - **`pthread_mutex`/`pthread_rwlock`/`pthread_cond`/`pthread_barrier`**:
    real and cross-process on **Linux** (non-private `FUTEX_WAIT`/
    `FUTEX_WAKE`, added alongside the existing `_PRIVATE` ops in
    `libc/src/wait.c` as `__crt_wait32_shared`/`__crt_wake32_*_shared` --
    the non-private operations key off the futex's physical backing
    (mapped page + offset) instead of `(mm_struct, virtual address)`, which
    is what makes two independent processes mapping the same
    `PTHREAD_PROCESS_SHARED` memory correctly rendezvous) and on **macOS**
    (`os_sync_wait_on_address`'s documented `OS_SYNC_WAIT_ON_ADDRESS_SHARED`
    / `OS_SYNC_WAKE_BY_ADDRESS_SHARED` flag bit, mirroring the Linux
    private/shared split -- reasoned carefully from the Windows-only session
    that wrote it, then verified for real on macOS hardware the same day:
    `tests/pthread_process_shared_test.c`'s real cross-thread contention
    passed cleanly, and two pre-existing tests (`pthread_attr_test.c`,
    `pthread_barrier_test.c`) turned out to still hardcode the pre-change
    "`setpshared(PTHREAD_PROCESS_SHARED)` always returns `ENOTSUP`"
    expectation -- fixed to gate on the same `CRT_PSHARED_SUPPORTED` split
    the implementation and the new test already use; see `HISTORY.md`).
    Stays `ENOTSUP` on **Windows**: `WaitOnAddress`/`WakeByAddressSingle`/
    `WakeByAddressAll` are documented by Microsoft as operating on the
    calling process's own virtual address space only, with no flag or
    variant that extends them cross-process -- an honest architectural
    limitation, not a missing feature, so a real fix there would need an
    entirely different mechanism (a named kernel object such as
    `CreateMutexA`/`CreateEventA`, or handle duplication/inheritance),
    out of scope for this primitive. `pthread_condattr_getpshared`/
    `setpshared` were also added (Bionic has these; this project didn't
    before), bit-packed alongside the existing clock-id storage in
    `pthread_condattr_t`.
  - **`pthread_spinlock`**: real and unconditional on **every host,
    including Windows**. Unlike the other primitives, this project's
    spinlock never calls into an OS wait/wake primitive at all --
    lock/trylock/unlock are pure `__atomic_*` builtins on a plain `int`,
    and atomic CPU instructions on genuinely shared memory behave
    correctly across process boundaries on every host this project
    targets. `pthread_spin_init()` no longer rejects
    `PTHREAD_PROCESS_SHARED` on any host.
  - Regression: `tests/pthread_process_shared_test.c` (new -- functional
    round-trip coverage including real cross-thread contention for mutex/
    rwlock/barrier/cond behind `#if CRT_PSHARED_SUPPORTED`, matching the
    exact Linux/macOS-only gate in `libc/src/pthread.c`; on Windows verifies
    the `ENOTSUP` contract instead) plus an updated `tests/
    pthread_spin_test.c` (now exercises a real pshared spinlock round trip
    on every host, including Windows).

## Lower priority: general POSIX/Bionic completeness, no identified near-term consumer yet

All six items originally listed here are now **done** (2026-08-17) -- see
[`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)'s entries for the full per-item writeups. Summary:

- **`uchar.h`** -- `mbrtoc16`/`c16rtomb`/`mbrtoc32`/`c32rtomb`, layered on
  the existing `mbrtowc()`/`wcrtomb()` UTF-8<->UTF-32 codepoint conversion
  (this project's `wchar_t` is a 32-bit codepoint on every host via
  `-fwchar-type=int`). `mbrtoc16`/`c16rtomb` add real UTF-16 surrogate-pair
  handling for codepoints outside the BMP.
- **`threads.h`** -- C11 thin wrapper over pthreads, matching real Bionic's
  own approach. `thrd_create()` adapts `thrd_start_t`'s `int(*)(void*)`
  signature to `pthread_create()`'s `void*(*)(void*)` via a small heap
  shim. Added `pthread_mutex_timedlock()` as a real new Bionic-parity
  primitive `mtx_timedlock()` needed.
- **`sys/prctl.h`** -- real on Linux (raw `prctl` syscall trampoline,
  x86_64=157, aarch64=167 -- reasoned carefully, flagged unverified
  pending real Linux hardware/CI), `ENOSYS` on macOS/Windows since `prctl`
  is a Linux-only kernel concept (real Bionic's own header is
  Linux-specific too).
- **`glob.h`** -- real `glob()`/`globfree()` built on
  `opendir`/`readdir`/`fnmatch`/`stat`. Supports `GLOB_APPEND`/
  `GLOB_DOOFFS`/`GLOB_ERR`/`GLOB_MARK`/`GLOB_NOCHECK`/`GLOB_NOSORT`/
  `GLOB_NOESCAPE`, multi-component wildcard patterns, and the standard
  hidden-dotfile convention. Implementing this surfaced two real,
  previously-undetected bugs with no prior regression coverage: `fnmatch()`
  had an inverted end-of-pattern match/no-match return (broke every
  pattern whose trailing wildcard needed that base case -- e.g.
  `fnmatch("*.txt", "alpha.txt", 0)` always failed -- affecting every real
  consumer, including toybox's `find`/`grep`/`tar`), and `remove()` never
  handled directories (only ever called `unlink()`, so `remove()` on a
  directory always failed, contrary to the C standard). Both fixed with
  new regression tests (`tests/fnmatch_test.c`, a `stdio_file_test.c`
  case).
- **`ifaddrs.h`** -- real `getifaddrs()`/`freeifaddrs()`, IPv4-only on
  every host (matching this project's existing AF_INET/AF_UNIX-only
  sockaddr translation scope on macOS). Linux via `/sys/class/net` +
  `SIOCGIFADDR`/`SIOCGIFNETMASK`/`SIOCGIFBRDADDR`/`SIOCGIFFLAGS` ioctls
  (reasoned, unverified pending real Linux hardware); macOS via the real
  Darwin `getifaddrs()` resolved at runtime plus sockaddr translation;
  Windows via `GetAdaptersInfo()` (IPv4-only itself), verified directly.
- **`ucontext.h`** -- real `getcontext()`/`setcontext()`/`makecontext()`/
  `swapcontext()` on every host and both architectures (x86_64/aarch64) --
  unlike the other items above, there is no honest host-level reason to
  hold any host back here: the mechanism (save/restore the callee-saved
  register set + stack pointer + resume address) is pure userspace state,
  mirroring this project's own already-verified `setjmp`/`longjmp`
  assembly. `makecontext()` supports up to 4 pointer-width arguments (a
  real, documented scope limit -- the number of integer argument
  registers Windows x64 has, used uniformly on every ABI for one simple
  bootstrap-frame layout). A real coroutine round-trip test
  (`tests/ucontext_test.c`) caught and fixed two genuine bugs on Windows
  x86_64 during development: a struct-layout bug (`long`/`unsigned long`
  are 4 bytes on Windows' LLP64 data model, not 8 like Linux/macOS's
  LP64, breaking the fixed-byte-offset bootstrap frame the trampoline
  assembly indexes into -- fixed with explicit `int64_t`/`uint64_t`), and
  a `swapcontext()` resume-point bug (its SP slot was saved
  `%rsp`-plus-8-adjusted, matching `getcontext()`'s convention, but its
  own resume label completes via a real `retq` rather than a raw `jmp` --
  `retq` needs the *unadjusted* stack pointer to correctly pop the real
  return address; saving the adjusted value caused it to jump to garbage
  instead). Windows also needs its thread's TEB (`NT_TIB.StackBase`/
  `StackLimit`/`DeallocationStack`) kept in sync with whichever stack is
  currently running -- a real requirement Linux/macOS don't have at all,
  found by the same coroutine test crashing until it was added. Verified
  directly on Windows x86_64 the day this was implemented; aarch64 (all
  three hosts: Linux/macOS/Windows) was reasoned carefully from the same
  proven register set but not run on real hardware until the next day
  (2026-08-17), when `tests/ucontext_test.c` hung indefinitely (100% CPU,
  no crash, no output) on real macOS aarch64 hardware -- a third genuine
  bug, this time in every aarch64 `ucontext.S` variant: `swapcontext()`
  patched the saved link-register slot with a resume-stub address the
  same way the x86_64 files use their own `1:` stub, but AAPCS64's `ret`
  branches through the x30 *register* rather than popping a return
  address off the *stack* the way x86_64's `retq` does, so nothing ever
  advanced x30 past the stub's own address -- the stub's final `ret`
  jumped straight back into itself, forever. Fixed by removing the
  resume-stub patch entirely (the naturally-saved return address already
  does the job a plain `ret` needs) and moving the "resumed context
  appears to return 0" behavior to an unconditional `mov w0, #0` right
  before every point this file jumps into a restored context instead;
  see `HISTORY.md`. Verified for real on macOS aarch64 (this host);
  Linux aarch64 and Windows aarch64 get the identical fix reasoned to
  apply the same way (same AAPCS64 mechanism) but not independently
  re-run on real hardware for those two.

`wordexp.h`/`nl_types.h`/`aio.h` remain lower priority still, not covered
by this sweep -- real Bionic either doesn't implement them meaningfully
(`wordexp`/`aio_*` are effectively stubs even on Android) or they have no
plausible graphics-stack consumer.

Already known and tracked elsewhere, not new findings, listed here only for
completeness against this same sweep:

- **C++ exceptions/RTTI across the runtime boundary** -- this was deferred at
  audit time. It is now implemented in the imported libc++/libc++abi lane and
  passes static/shared smoke on all three hosts; the small bootstrap `cxx`
  target deliberately remains `-fno-exceptions -fno-rtti`. See
  `docs/design/cxx_runtime.md`.
- **`pthread_cancel`** -- declared, but a real `ENOTSUP` stub
  (`libc/src/pthread.c`), matching `docs/guides/import_bionic.md`'s "Cancellation
  and robust mutexes are still deferred."

## AF_UNIX SOCK_SEQPACKET and message flags on macOS (2026-10-12, revised after review)

Darwin has no sequenced-packet AF_UNIX sockets (`EPROTONOSUPPORT`). CRT-provided, in `libc/src/socket.c` (macOS
block) with hooks in `libc/src/fd.c` and `libc/src/process.c`: a `SOCK_STREAM` socket with 4-byte length-prefixed
records, `SCM_RIGHTS` sent in the same `sendmsg()` as the length. Both peers must be CRT.

- *State.* One `struct seq_socket` per framed socket, shared by every descriptor made with `dup`/`dup2`/`F_DUPFD`:
  a send lock (a record is written by one thread at a time, also across `dup`ed descriptors), a receive lock, and the
  partly received record (header bytes, payload, the descriptors that arrived with it). Descriptors the table cannot
  hold (>= 4096) are refused with `EMFILE` for `socket`/`accept`/`dup`, never silently left as a plain stream.
- *Receive.* The kernel is only ever read non-blockingly; the call waits (`poll`) only when the caller is blocking.
  With `O_NONBLOCK` or `MSG_DONTWAIT` a half header or half payload is `EAGAIN` and the state is kept for the next
  call. The whole record is buffered before delivery; descriptors that arrived with it are owned by the state until
  then, so an oversize length (`EPROTO`, the connection is marked broken), a peer that closes mid-record
  (`ECONNRESET`) or a record that is never delivered closes them. Delivery copies as many whole descriptors as the
  caller's control buffer holds, closes the rest and sets `MSG_CTRUNC`; `MSG_CMSG_CLOEXEC` is applied; `MSG_TRUNC`
  follows Linux (the return value is the real length when the flag is passed); `MSG_PEEK` is `EOPNOTSUPP`.
- *Send.* A sender that could not write a single byte gets `EAGAIN` with nothing sent (a whole-record retry works).
  Once any part of a record is on the wire it is completed, waiting for `POLLOUT` whatever the socket's mode, because
  a half record would corrupt the stream. This is a documented difference from a kernel SEQPACKET (where a record is
  accepted or refused whole); a caller that must never block mid-record should keep records small.
- *Vectors.* `readv`/`writev` on a framed socket are one `recvmsg`/`sendmsg`, i.e. one record over all vectors.
- *exec.* The framing flag lives in the process, so the spawn/exec paths (`posix_spawn`, `posix_spawnp`, `execve` and
  what calls it) list the framed descriptors that are still sockets, after the file actions renumbered them, in
  `CRT_SEQPACKET_FDS` of the new environment; `__crt_env_set_initial()` of the new image reads and removes it. A child
  that is not CRT ignores the variable. `fork` copies the table and resets the locks in the child.
- *Passed sockets.* A framed socket that is itself sent in an `SCM_RIGHTS` message would arrive in the receiving process
  as a plain stream (the flag is per process), and its framed peer would take the raw bytes for a record length (found by
  WebKit's Network/Web process connection, B4 on macOS). Bit 31 of the length word announces that an 8-byte mask follows,
  one bit per descriptor of the record that is framed in the sender; the receiver frames those descriptors as it delivers
  them. Records without framed descriptors keep the 4-byte header, and an oversize length is rejected before any mask is
  awaited.
- *Limits.* 64 MiB per record, 64 iovecs and 64 descriptors per record, descriptors below 4096; `MSG_PEEK` unsupported.

Separately, `recv`/`send`/`recvmsg`/`sendmsg` flags were passed to Darwin with Linux numbers (`MSG_DONTWAIT` arrived
as `MSG_WAITALL`); `MSG_EOR`, `MSG_TRUNC`, `MSG_CTRUNC`, `MSG_NOSIGNAL` are translated, `MSG_CMSG_CLOEXEC` is applied to
received descriptors, and Darwin's un-clamped `cmsg_len` after `MSG_CTRUNC` is clamped. Tests: `unix_socket_test`
(Gate B, plus on macOS: partial header/payload with `O_NONBLOCK` and `MSG_DONTWAIT`, descriptor cleanup for an oversize
record and for a peer that closes mid-record, `readv`/`writev` records, four concurrent senders of 70 KB records,
descriptor inheritance across `posix_spawn`, duplication at the table limit; each fails when the behaviour it covers is
removed), `socketpair_test`, `crtweb_wire_transport_test`.

## Process spawning and `/proc/self/exe` (2026-10-12)

Found by the macOS replay of B4. `posix_spawn()`/`posix_spawnp()` on Linux and macOS are fork + exec, and a failure after
the fork (the exec itself, a file action, the attributes) was an `_exit(127)` the caller never heard of; Bionic and glibc
return the errno. A close-on-exec status pipe, its write end moved above descriptor 192, carries it back; the parent reaps
the child and returns the value. On macOS `readlink("/proc/self/exe")` read the executable file as if it were a link
(`EINVAL`); it returns the canonical path. `strerror` gained the Bionic messages for `EPROTO`, `EBADMSG`, `ESTALE` and the
other errno values it lacked. Tests: `spawn_status_test`, `unix_socket_test`.

## Process kill and socket lifetime on Windows (2026-10-12)

Found by the Windows replay of B4 (`libc/src/arch/windows/common/syscall.c`).

- *`kill()`.* Windows has no signals. `kill(pid, 0)` probes the process (`OpenProcess`); `SIGKILL` is
  `TerminateProcess` with an exit code `0x4B110000|sig`, and `waitpid` maps that tag to `WIFSIGNALED`/`WTERMSIG`.
  Other signals return `ENOSYS`. A native Windows process killed this way reports the tag too. Test: `spawn_kill_test`.
- *`readlink("/proc/self/exe")`* returns `GetModuleFileName` with forward slashes, no NUL, truncated to the buffer.
- *Sockets passed to another process* (`SCM_RIGHTS`, the `posix_spawn` descriptor snapshot) are `DuplicateHandle`d into
  the target; the receiver runs `WSAStartup` before using the handle. `WSADuplicateSocket` was used before: its copy is
  destroyed with the sending process, so a descriptor in flight died with the sender.
- *Close.* An abortive close of the last handle of an AF_UNIX socket discards unread data and resets the peer. The last
  handle (`NtQueryObject` `HandleCount == 1`), on `close` and at `exit`, is `shutdown(SD_SEND)` first so the peer reads
  its data and then sees EOF. Limit: a handle held by another process is not counted as "last".
- *Inheritance.* CRT sockets are non-inheritable; a handle inherited by an unrelated native child kept connections alive
  past the exit of their owners. Descriptors reach CRT children through the spawn snapshot, not handle inheritance.

## IPv6 on macOS (2026-10-11)

Found by turning IPv6 back on in libcurl: the macOS socket layer translated only `AF_INET` and `AF_UNIX`.
`AF_INET6` (Bionic 10, Darwin 30), `struct sockaddr_in6` (Darwin's length-prefixed layout), `IPPROTO_IPV6` option
numbers, `inet_pton`/`inet_ntop` for IPv6 on every host, `in6addr_any`/`in6addr_loopback`, and a `getaddrinfo()` that
returns IPv6 results as a list are now provided; details and tests in `docs/acceptance/crtweb_acceptance.md` (B2 on
macOS). Still missing: interface-name zone ids, `IPV6_JOIN_GROUP`/packet-info options, AAAA in the built-in resolver.

## Long argument vectors and thread_local destructors on Windows (2026-10-12)

- *Argument vectors beyond the 32,767-character command line.* Linux allows ARG_MAX (2 MiB); CreateProcess does not.
  A spawn of a CRT program with a longer vector (OpenSSL's Makefile archives 1,500 objects in one `ar` line) used to
  fail with `E2BIG`. `__crt_sys_posix_spawn` now spills the vector to a private temp file, passes only `argv[0]` on
  the command line and names the file in `CRT_ARGV_FILE`; the child's startup reads it, deletes it and unsets the
  variable. A native Windows program has no startup to do that and keeps the limit. Environment blocks are not spilled.
- *`__cxa_thread_atexit` and `__dso_handle`.* libc++abi builds `cxa_thread_atexit.cpp` for UNIX only, so a Windows image
  with a non-trivially destructible `thread_local` had an undefined reference (invisible to an ELF `--gc-sections`
  link, reported by COFF). libc now provides `__cxa_thread_atexit` (pthread key, per-thread LIFO list, nested
  registrations run first, `atexit` for the thread that calls `exit()`), and `crt1.o` a weak `__dso_handle`.

## AF_UNIX on Windows (2026-10-12)

Native (Winsock, Windows 10 1803+): stream sockets bound to a file system path. CRT-provided on top of it
(`libc/src/arch/windows/common/syscall.c`): `sockaddr_un` path mapping, `socketpair()`, `SOCK_SEQPACKET` (a framed
stream socket, CRT-to-CRT) and `SCM_RIGHTS` on it (files, pipes, memfds through `DuplicateHandle`, sockets through
`WSADuplicateSocket`, addressed by the peer's process id). Not provided: `SOCK_DGRAM`, abstract names, ancillary data
on a stream socket (`ENOTSUP`), `MSG_PEEK` on `SOCK_SEQPACKET`, other descriptor kinds. Tests: `unix_socket_test`,
`socketpair_test`, `crtweb_wire_transport_test`; design notes in `docs/acceptance/crtweb_acceptance.md`.

## IPv6 and socket option numbering on Windows (2026-10-12)

The Windows PAL passed Bionic numbers to Winsock: `AF_INET6` (10 here, 23 there) failed, and so did the
`SOL_SOCKET` options other than reuse/error/type. Now translated in `__crt_sys_*` (family field of every sockaddr in
both directions, the `SOL_SOCKET` and `IPPROTO_IPV6` tables, `struct linger`); an unmapped option is `ENOPROTOOPT`.
Still missing on Windows: AAAA in the built-in resolver (a real name resolves from A records), interface-name zone ids,
`IPV6_JOIN_GROUP`/packet-info, `IP_*` level option numbers (`IP_TTL`, `IP_MULTICAST_*` differ between Bionic and
Winsock and are passed through), `SO_RCVTIMEO`/`SO_SNDTIMEO` (a `timeval` here, milliseconds there), `SO_REUSEPORT`.

## Found by the web-stage ports (2026-10-10)

OpenSSL 3.5's `BIO_ADDR`, `bio_sock2` and `bss_dgram` (Web Tranche 3B, milestone B2) needed the
following Bionic surface, all added: `getpeername()` (Linux and macOS syscalls, Winsock), `SO_TYPE`
and `struct linger` in `<sys/socket.h>`, `getnameinfo()` with `NI_*`, `struct servent` with
`getservbyname()`/`getservbyport()` (a built-in table: no `/etc/services` on Windows) and
`inet_ntoa()`. `getnameinfo()` has no reverse-DNS client: the host is always numeric, and
`NI_NAMEREQD` fails with `EAI_NONAME`. `<sys/shm.h>` (SysV shared memory) is *not* provided, as in
Bionic: the OpenSSL recipe compiles with `-D__ANDROID__=1` so OpenSSL takes its Android branch
(no SysV shm, no ucontext async), which is the branch that matches a Bionic-compatible libc.
Earlier the same day the web surface protocol had added `SOCK_SEQPACKET`, `MSG_TRUNC`, `MSG_CTRUNC`
and `socketpair()`. Covered by the OpenSSL and curl-openssl port tests and `socketpair_test`.

## Found by the WebCore build (Web Tranche 3B, milestone B3; 2026-10-11)

Building fontconfig, libwpe and WebCore against the SDK showed these gaps, all fixed in CRT:

- `<sys/time.h>`: `timerclear`, `timerisset`, `timercmp`, `timeradd`, `timersub` (Bionic has them;
  fontconfig's cache code uses `timercmp`).
- `<stdlib.h>`: `initstate()` and `setstate()` (fontconfig's `FcRandom`). The generator has one
  feedback polynomial, so the buffer holds the 48-bit state of one independent sequence and the size
  only has to be >= 8 bytes (BSD selects among several polynomials by size).
- `<sys/mman.h>`: `shm_open()` and `shm_unlink()`. Bionic has neither (Android uses ashmem); WebKit's
  `SharedMemoryUnix.cpp` falls back to them after `memfd_create()`. Implemented as files named
  `crt_shm_<name>` in the temporary directory, the way `memfd_create()` is; not a kernel object.
- `<android/log.h>`: `__android_log_buf_write` was declared and not defined. `__android_log_write`,
  `__android_log_print`, `__android_log_vprint`, `__android_log_buf_print`, `__android_log_assert`
  and `log_id_t` now exist; records go to standard error as `P/tag: text`. There is no separate
  `liblog`, so libwpe is configured with `HAVE_ANDROID_LOG_H=0` (a link of `-llog` would fail).
- Toolchain, not libc: a shared-runtime C++ executable that contains static destructors (any large
  static library) failed with `undefined reference to __dso_handle`, because `libc.so`'s is hidden;
  `tools/crt-c++` now links `crtbegin_so.o` into such executables as it does into shared objects. And
  `libc++abi.so`/`libicuuc.so` call the loader's `__tls_get_addr`, which resolves only if the loader
  is a `DT_NEEDED` object (glibc's `libc.so.6` is one, CRT's `libc.so` is not): the wrapper adds an
  empty stub carrying the loader's soname, so a `thread_local` with a destructor
  (`__cxa_thread_atexit`) no longer dies with a symbol lookup error. Neither change links a host
  library.

Covered by `random_state_test`, `shm_open_test`, `android_log_test`, the fontconfig and libwpe port
tests and `crtweb_webcore_graph_test`.

