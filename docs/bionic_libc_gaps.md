# Bionic Libc Completeness Gaps Before `libcrtgfx`

> **Status:** This is the completed pre-`libcrtgfx` audit ledger, not the
> current work queue. Every high/medium/lower-priority item identified by this
> sweep was subsequently implemented; later real-host corrections and
> verification are recorded in `HISTORY.md`. Current limitations live in
> `STATUS.md`/`TODO.md`. Caveats below are preserved as evidence of what was
> known at the time each tranche landed.

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
    project has no real ELF dynamic linker yet (`docs/dynamic_loading.md`:
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
`HISTORY.md`'s entries for the full per-item writeups. Summary:

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
  `docs/cxx_runtime.md`.
- **`pthread_cancel`** -- declared, but a real `ENOTSUP` stub
  (`libc/src/pthread.c`), matching `docs/import_bionic.md`'s "Cancellation
  and robust mutexes are still deferred."
