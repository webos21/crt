# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## note

- **Recipe/port status upkeep.** Keep recipe statuses
  (`porting/recipes/*.json`, `docs/porting_status.md`) current as each
  host is rerun.

- **A local dev tree with an existing `out/` directory is not a reliable
  test of new CMake-level wiring -- this has now caused two separate
  real CI-only failures, verify with a genuinely fresh tree before
  calling any such change done.** This project's own dev machines keep
  a long-lived `out/<preset>` around across sessions, so `CRT_ROOTFS`
  (and anything else set via `CACHE` variables, or files/targets a
  prior build already produced) is usually already sitting in
  `CMakeCache.txt`/on disk from an earlier configure -- masking
  ordering bugs (a variable referenced before the line that sets it
  runs, in top-level-`CMakeLists.txt`-vs-`add_subdirectory()` order; a
  `DEPENDS`/`add_dependencies()` that only orders against an aggregate
  target's own completion, not the sibling DAG nodes that actually
  consume the dependency's output) that a truly fresh checkout -- every
  CI run, unconditionally -- cannot paper over. Happened twice now: the
  `windows_pseudo_reloc_test` `DEPENDS` gap (2026-08-12, see
  `HISTORY.md`) and the `CRT_ROOTFS` subdirectory-ordering gap
  (2026-08-16, see `HISTORY.md`) -- both root-caused only after
  reproducing locally from a genuinely fresh clone, both invisible on
  this exact dev tree beforehand. **Before considering any change to
  `CMakeLists.txt`/`tests/CMakeLists.txt`/`shell/CMakeLists.txt` (new
  `ENVIRONMENT`/`DEPENDS` test properties, new `CACHE` variables, new
  cross-`add_subdirectory()` references) actually done, verify it
  either via a fresh `git clone` into a scratch directory, or at
  minimum `cmake --fresh --preset <preset>` in place (discards
  `CMakeCache.txt` without a full `out/` wipe) -- not just an
  incremental `cmake --build` against whatever's already configured.**

- **`porting/recipes/mbedtls-windows-exclude-symbols.rsp` is a hand-
  generated snapshot of every public/`__crt_sys_*` libc symbol name, not
  something regenerated automatically at build time -- it silently drifts
  stale every time a new libc symbol is added anywhere, and nothing
  catches that until some port's Windows DLL link happens to pull in the
  new symbol's translation unit (each `libc/src/arch/windows/common/
  syscall.c` symbol is `-Wl,--exclude-symbols`-suppressed individually,
  and that file compiles as a single translation unit, so pulling in
  *any* one of its symbols pulls in the whole `.obj`, exports and all).
  Happened for real (2026-08-17, see `HISTORY.md`): building mbedtls
  failed with `ld.lld: error: duplicate symbol: __crt_sys_sendmsg` (also
  `__crt_sys_recvmsg`/`__crt_sys_link`) purely because this session's
  earlier `sendmsg`/`recvmsg`/`link` work never touched this file. Fixed
  by regenerating the full list from a fresh `llvm-nm --defined-only -g`
  dump of `lib/c.lib` (917 -> 947 entries), but the underlying gap is
  still open: **whenever a new public or `__crt_sys_*` libc symbol is
  added, regenerate this `.rsp` file in the same pass** (or, better,
  replace the checked-in snapshot with a real build-time generation step
  so this stops being a manual step to remember at all) -- don't wait for
  a port to hit it.

- **Standing porting-loop discipline**, not a task list:
  1. expose the missing header/type/macro/symbol/behavior with upstream
     source;
  2. check Android Bionic public headers, source, ABI, and errno policy;
  3. extend CRT/PAL/sysroot rather than patching upstream first;
  4. record host-specific policy differences in `docs/`;
  5. **verify both the static AND shared build during the same porting
     pass, on every host, before calling a port done** -- not
     static-first-then-shared-as-a-follow-up. Several ports in this
     queue (bzip2, xz, pcre2, mbedtls) landed `static-pass` first and
     only got `shared-pass` in a later pass or after the user asked why
     shared hadn't been checked; going forward, a port's recipe/test
     entries and status write-up should cover both build shapes before
     the port is reported as finished, and a host-specific reason must
     be recorded in the recipe's own notes if shared is genuinely
     deferred for that host (e.g. a real missing SDK import library),
     not just left unmentioned.

  A few smaller, longer-running audits ride along with this:
  - Keep auditing disabled toybox applets for pointer-to-`long` LLP64
    assumptions before enabling them (see `HISTORY.md`'s `which`/
    `readlink`/`stat` and `id`/`xargs` entries for the most recent
    batches actually enabled).
  - Keep `/dev/tty`, `/dev/console`, `isatty`, `tcgetattr`, `tcsetattr`,
    and `TIOCGWINSZ` behavior coherent enough for non-interactive shell
    and configure use. Windows' `tcgetattr`/`tcsetattr` round-trip
    fidelity (a per-fd shadow so a value `tcsetattr()` was asked to set
    comes back verbatim from `tcgetattr()`, not re-derived from hardcoded
    defaults every call) was fixed 2026-08-16 -- see `HISTORY.md`. Linux's
    and macOS's own `tcgetattr`/`tcsetattr`/`tcdrain`/`tcflow`/`tcflush`/
    `tcsendbreak` were pure hardcoded-value/no-op stubs (a different, more
    complete gap than Windows' -- not just round-trip fidelity, no real
    ioctl at all) until real `TCGETS`/`TCSETS*`/... (Linux) and
    `TIOCGETA`/`TIOCSETA{,W,F}`/... (macOS) ioctl-backed ports landed
    2026-08-17 -- see `HISTORY.md`. Windows' own `tcdrain`/`tcflow`/
    `tcflush`/`tcsendbreak` had the exact same gap (pure `isatty()`-check-
    then-no-op stubs, real Win32 backing never wired up) and are now fixed
    the same day: `tcdrain()`/`tcflush(TCIFLUSH/TCIOFLUSH)` call real
    `FlushFileBuffers()`/`FlushConsoleInputBuffer()`; `tcflow()`/
    `tcsendbreak()` stay honest no-ops (once a real tty fd is confirmed)
    since a Windows console genuinely has no serial-line-shaped flow-
    control or break-condition concept to back them with, matching this
    same note's own `TIOCGWINSZ` precedent below. `TIOCGWINSZ` still
    legitimately returns `ENOTTY` when the console has no real output
    screen buffer (confirmed directly: this project's own dev environment
    has an attached console for input but `GetConsole ScreenBufferInfo`
    fails on it) -- correct behavior for that real condition, not a bug,
    but it means `stty -a`/`stty size` can't be exercised end-to-end in
    every environment.
  - Continue validating that `CRT_SPAWN_NATIVE_WINDOWS=1` stays a narrow
    launcher hint for native host tools (LLVM `ar`/`ranlib`/`strip`), not
    an inherited global mode for configure recipes.

## done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## in progress

Active threads, not a flat list of one-off items. Remaining libc/PAL
residuals before the upper runtime phase (see `docs/runtime_roadmap.md`):

### Bionic libc completeness before `libcrtgfx`

Reviewed (2026-08-16) against real Android Bionic's public surface, not
guessed -- full findings, evidence, and priority tiers in
[`docs/bionic_libc_gaps.md`](docs/bionic_libc_gaps.md). Summary:

- All four "high priority" items are **done** (2026-08-16): `semaphore.h`,
  public `<stdatomic.h>`, `sendmsg`/`recvmsg` + `SCM_RIGHTS`/`CMSG_*` fd
  passing, and `memfd_create`. See `HISTORY.md`. The `sendmsg`/`recvmsg`
  raw syscall trampolines' real-hardware caveat is now closed (2026-08-17):
  `tests/sendmsg_scm_rights_test.c`'s real AF_UNIX fd-passing round trip
  ran on real macOS hardware and found four real ABI bugs (AF_UNIX
  sockaddr translation, `struct msghdr` field widths, `CMSG_ALIGN` unit,
  `cmsg_level`/`SOL_SOCKET` translation), all fixed and verified -- see
  `HISTORY.md`'s 2026-08-17 entry. Linux was not re-verified this pass
  (only macOS was available); its trampolines remain reasoned-not-verified
  until they actually run on real Linux hardware/CI.
- **Medium priority**: `sys/epoll.h`/`sys/eventfd.h`/`sys/timerfd.h` are
  **done** (2026-08-17) -- Linux-only, matching real Bionic exactly;
  declared on every host (`ENOSYS` on macOS/Windows) with real raw Linux
  syscall trampolines, reasoned carefully but **not yet independently
  verified on real Linux hardware** (same open caveat as `sendmsg`/
  `recvmsg` had before real macOS testing closed theirs -- see
  `docs/bionic_libc_gaps.md` for the full writeup, including a real
  x86_64-vs-aarch64 `struct epoll_event` layout difference that needed
  care). `dl_iterate_phdr`/`link.h`/`elf.h`/`dladdr` are also **done**
  (2026-08-17) -- real per-host implementations, not stubs, wherever each
  host actually has something real to report (Linux: the main
  executable's own real `AT_PHDR`/`AT_PHNUM`-derived data, one entry, no
  real ELF dynamic linker exists yet to report more; macOS/Windows:
  `dl_iterate_phdr()` honestly reports zero ELF images since Mach-O/PE
  have no `Elf64_Phdr` equivalent at all, while `dladdr()` is real on both
  via each host's own real image-introspection API) -- see
  `docs/bionic_libc_gaps.md` for the full per-host writeup; verified
  directly on Windows, Linux/macOS reasoned carefully but not yet run on
  real hardware from this session. `PTHREAD_PROCESS_SHARED` is also
  **done** (2026-08-17) -- real and cross-process on Linux (non-private
  futex ops) and macOS (`os_sync_wait_on_address`'s `SHARED` flag,
  reasoned from a Windows-only session and then verified for real on
  macOS hardware the same day -- `tests/pthread_process_shared_test.c`'s
  real cross-thread contention passed, and two pre-existing tests that
  still hardcoded the pre-change `ENOTSUP` expectation for
  `setpshared(PTHREAD_PROCESS_SHARED)` were fixed; see `HISTORY.md`),
  unconditional on every host for `pthread_spinlock` (pure atomics, no OS
  wait/wake call to begin with), and an honest `ENOTSUP` on Windows for
  the other four primitives
  (`WaitOnAddress`/`WakeByAddress*` are documented same-process-only with
  no cross-process capability to opt into) -- see `docs/bionic_libc_gaps.md`
  for the full per-host writeup. This closed out every medium-priority
  item; see the next bullet for the lower-priority tier.
- **Lower priority, no identified near-term consumer**: also **done**
  (2026-08-17) -- `uchar.h`, `threads.h`, `sys/prctl.h`, `glob.h`,
  `ifaddrs.h`, `ucontext.h`. Real implementations throughout, not stubs;
  see `docs/bionic_libc_gaps.md`'s "Lower priority" section for the full
  per-item writeup and `HISTORY.md` for the implementation trail.
  Implementing `glob.h` surfaced and fixed two real, previously-
  undetected bugs with no prior regression coverage (`fnmatch()`'s
  inverted end-of-pattern match logic, `remove()` never handling
  directories). Implementing `ucontext.h` surfaced and fixed two real
  bugs on Windows x86_64 (an LLP64 struct-layout mismatch, and a
  `swapcontext()` resume-point bug using an adjusted stack pointer with a
  `retq`-based resume path that needs the unadjusted one) via a real
  coroutine round-trip test. This closes out **every** item from the
  2026-08-16 Bionic libc gap audit -- high, medium, and lower priority
  alike.
- Already known/tracked elsewhere (not new findings): C++ exceptions/RTTI
  across the runtime boundary (`docs/cxx_runtime.md`), `pthread_cancel`
  (a real `ENOTSUP` stub).

## planned

### Upper runtime roadmap after libc/PAL cleanup

The long-term target is an Electron-class rebuilt native application runtime,
not Electron itself as the next port. See `docs/runtime_roadmap.md`.

- **libcrtjs**: start with QuickJS to expose event-loop, module-loading,
  filesystem, timer, native-binding, and process gaps at manageable scale.
  Keep V8 as the final browser-class JavaScript engine target after the C++
  runtime, JIT/code-memory policy, atomics, threading, and dynamic loading are
  stronger.
- **libcrtgfx**: build toward Skia plus a Wayland-compatible compositor
  boundary, with a Chromium Ozone backend as the long-term browser integration
  path. Host window/GPU APIs stay below the graphics PAL.
- **libcrtmedia**: build toward FFmpeg and explicit codec/audio/video
  libraries, with software decode first and later hardware acceleration through
  host backends that interoperate with `libcrtgfx`.

### Interactive job control (deferred until it's an actual priority)

`docs/job_control.md`'s "Interactive Job Control" section has the decided
design for all three pieces below; nothing here is implemented yet, and this
project's own mksh build has job control compiled out entirely on every host
(`MKSH_NOPROSPECTOFWORK`), not just Windows -- see that section for why this
is forward-looking policy, not a current gap being actively worked.
Re-evaluated (2026-08-16) against `docs/runtime_roadmap.md`: none of the
planned upper-runtime components (`libcrtjs`/QuickJS+V8, `libcrtgfx`, `libcrtmedia`)
actually depend on POSIX job-control signals (`SIGSTOP`/`SIGTSTP`/`SIGCONT`)
or real fg/bg switching -- confirmed genuinely optional infrastructure, not
something blocking the roadmap. (V8's own "signal/process behavior"
prerequisite in that doc is a separate matter -- `SIGSEGV`-trap-based WASM
bounds checks and `SIGPROF`-style profiling, the "vectored exception
handling" question `docs/signal_delivery.md` already tracks independently,
answerable with fully documented Windows APIs.) A full Windows stop/resume
implementation would also need reversing this project's "avoid undocumented
NT internals" pattern (`NtSuspendProcess`/`NtResumeProcess` -- see
`docs/job_control.md`'s own "Stopped-child status" note for the design that
was investigated and the alternatives ruled out). Stays deferred.

- Bridge `SetConsoleCtrlHandler` (`CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, both to
  `SIGINT`) into `signal_actions[]`/`raise()`, mirroring `SIGCHLD`'s existing
  pending-flag-plus-checkpoint pattern (`docs/signal_delivery.md`).
- Track the real Windows process-group id behind this project's own
  CRT-managed `pgid` integer once a job is actually spawned into a new
  process group, so `tcsetpgrp()` and a targeted `CTRL_BREAK_EVENT` have a
  real id to act on.
- Re-enable `MKSH_UNEMPLOYED` (mksh's own job control) once the above exists,
  and only then decide whether stopped-child (`WIFSTOPPED`) support is worth
  the low-level Windows work it would need -- `docs/job_control.md` currently
  keeps that explicitly out of scope.

### Toybox applet expansion (deferred until it's an actual priority)

Only when the backing Bionic-compatible CRT/PAL surface exists.
Full applet-by-applet status (what's enabled,
what's still open and why, the deferred-applet list with each one's
concrete reason, and the `globals.h`/`flags.h` registration traps found
while enabling `df`/`stty`) now lives in
[`docs/toybox_applet_status.md`](docs/toybox_applet_status.md) -- this
bullet stays a pointer. Still open there: `expand`/`logger`/`fold`/
`uudecode`/`cal`/`split`/`strings` (a `globals.h` fix, plus a per-applet
`flags.h` check); `timeout` (hang fixed, two deeper gaps remain: real
`SIGCHLD` `siginfo_t` data, cross-process `kill()`); and a confirmed-not-
guessed deferred list (`ps`/`top`/`iotop`/`pgrep`/`pkill`, `mount`/
`umount`, `ifconfig`, `login`, procfs-heavy commands).
