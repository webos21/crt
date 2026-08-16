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
    and configure use.
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

- Expand toybox applets only when the backing Bionic-compatible CRT/PAL
  surface exists. `which`/`readlink`/`stat`/`touch`/`id`/`xargs`,
  `cksum`/`crc32`/`tsort`/`tty`/`unlink`/`uuencode`, and `link` (needed a
  real `linkat()` PAL implementation first, not just an LLP64 audit -- see
  `HISTORY.md`) are done -- next candidates:
  - `expand`, `logger`, `fold`, `uudecode`, `cal`, `split`, `strings` are
    audited and LLP64-safe, but need a real `shell/toybox/src/android/
    linux/generated/flags.h` regeneration first (their `GLOBALS()` struct
    is missing from the committed `union global_union` entirely -- see
    `HISTORY.md`'s 2026-08-16 entry) -- toybox's own `mkflags`
    C-preprocessor pipeline (`scripts/make.sh`/`scripts/genconfig.sh`),
    not a hand-edit.
  - Beyond those, keep auditing the remaining disabled applets for LLP64
    pointer-width safety.

- **Verify `linkat()`'s new Linux x86_64/aarch64 and macOS x86_64/aarch64
  raw syscall trampolines with a real build and run on those hosts.**
  Added 2026-08-16 (see `HISTORY.md`) alongside the Windows
  `CreateHardLinkA` implementation, which *is* directly verified on real
  Windows hardware this session (`crt_mksh_rootfs_link_runs`). The other
  three hosts only got a hand-written `libc/src/arch/{linux,macos}/
  {x86_64,aarch64}/syscall.S` trampoline mirroring the existing
  `__crt_sys_symlink`/`__crt_sys_unlink` pattern in the same files, using
  well-established syscall numbers (Linux x86_64 `__NR_link`=86, Linux
  aarch64 `__NR_linkat`=37 via `AT_FDCWD`, Darwin `SYS_link`=9 on both
  archs) -- this dev environment has no cross-toolchain to even compile
  those three files, let alone run `crt_mksh_rootfs_link_runs` against
  them. Low-risk (mechanical mirror of an already-working pattern, stable
  decades-old ABI numbers) but genuinely unverified -- do not treat as
  "done" until a real Linux and macOS `ctest` run confirms it, per this
  project's own "always verify with a real build" discipline.
- Keep deeper Linux-like applets deferred until the PAL owns enough backing
  behavior:
  - `ps`: add through toybox only after the rootfs/PAL provides enough
    `/proc` process data; this is not an mksh builtin.
  - `mount`;
  - `df`;
  - `ifconfig`;
  - `stty`;
  - `login`;
  - device-manager or procfs-heavy commands.

## planned

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
