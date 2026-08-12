# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## done

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself --
when an item below is finished, move its writeup into `HISTORY.md` (dated,
newest entry first) rather than leaving it here.

## in progressing

Five active threads, not a flat list of one-off items:

- **Porting matrix expansion.** Queue, in order: `bzip2` -> `xz` -> `pcre2`
  -> `mbedtls` -> `curl` (`openssl` held back until something actually
  needs it). Any POSIX/rootfs gap a port's build exposes gets fixed in
  place as part of that port's own work, not deferred to a separate pass
  -- matching how zlib/libpng/libffi already surfaced and fixed real CRT
  gaps along the way (see `docs/porting_status.md`). Each port's recipe
  and per-host status live in `porting/recipes/*.json` and
  `docs/porting_status.md` as they land; this bullet just tracks the
  overall queue position.
  - `bzip2`: done on Linux and Windows (`shared-pass` both, verified with
    a real compress/decompress round trip against both the static and
    shared build, plus an `ldd` rpath check on Linux). No new CRT/PAL gap
    surfaced -- it built cleanly against the existing sysroot the same
    way sqlite-amalgamation already does. macOS still `pending` (no
    macOS hardware available this session).
  - `xz` (liblzma): **done on Linux and Windows, both `shared-pass`**
    (macOS still `pending`, no macOS hardware available this session). A
    real compress/decompress round trip at preset 9|EXTREME with CRC64
    (real match-finder allocations, byte-for-byte output compare)
    verified against both static and shared builds on both OSes. Two
    genuine, general CRT/toolchain gaps got found and fixed along the
    way, not deferred -- see `HISTORY.md`'s dated entries and
    `porting/recipes/xz.json`'s own notes for the full trail: (1) this
    project's `crt1` startup never ran ELF/PE constructor sections at
    all (`.init_array`/`.ctors`/`.CRT$XCU` depending on OS/ABI) for the
    executable entry point on any OS -- see the general fix below; (2)
    `lld-link` doesn't reliably merge constructor sections contributed by
    a *static archive* (as opposed to a directly-compiled object) the way
    GNU ld's default script guarantees, worked around via a recipe patch
    routing liblzma onto its own portable non-constructor fallback path
    on Windows specifically.
    Next: `pcre2`.

- **`.init_array`/`.fini_array` (ELF/PE/Mach-O constructor/destructor)
  support -- DONE on all three OSes, see `HISTORY.md`'s dated entries for
  the full investigation.** This project's `crt1` startup never ran
  `__attribute__((constructor))`-registered functions (also what runs
  C++ global object constructors) for the executable entry point on any
  OS, found while porting xz/liblzma; invisible until then because every
  prior port linked shared, where the OS's own dynamic loader handles a
  `.so`'s own constructors automatically. Linux and Windows both fixed
  and verified via a full local `ctest` run; macOS confirmed directly on
  real hardware (`tests/init_array_test.c` prints `init_array_test: ok`)
  requiring no new code at all, since dyld already runs Mach-O
  constructors automatically and this project's existing
  `__cxa_finalize(0)` call in `exit.c` already handled destructors.
  Windows needed bracketing two entirely separate section conventions
  simultaneously (GNU `.ctors`/`.dtors` for `tools/crt-cc` port builds,
  MSVC `.CRT$XCU`/`.CRT$XTX` for this project's own CMake-native builds)
  and surfaced/fixed a real, pre-existing, unrelated latent bug in the
  startup self-relaunch path (`fork_capable_relaunch.c`, now `_exit()`
  instead of `exit()`). A permanent regression test,
  `tests/init_array_test.c`, now guards the general mechanism on every
  OS's `ctest` run going forward. The one residual, documented
  limitation: `lld-link` doesn't reliably bracket a constructor
  contributed by a *static archive* on Windows (worked around for xz via
  a recipe patch, not a general CRT-level fix) -- any future port whose
  own static-archive code relies on `__attribute__((constructor))` on
  Windows will need the same kind of targeted, per-recipe fix.

- **Windows shell/process stress hardening.** Real concurrency -- parallel
  `make -jN`, jobserver pipe fd handling, many live children in the
  registry at once, subshell/redirection edge cases -- was never actually
  exercised on Windows until this thread opened; every Windows port build
  had always run serial `make -j 1`.
  - **Parallel `make -jN` on Windows is concluded: root-caused, fixed,
    stress-tested at libpng scale, and enabled by default -- see
    `HISTORY.md`'s 2026-08-11 entries for the full investigation.** Both
    the fatal `make.exe: /system/bin/mksh: Bad file descriptor`/
    `Error 127` crash and the jobserver token-count mismatch it was
    originally bundled with traced back to the same bug
    (`__crt_sys_fstat()` destructively `ReadFile()`-ing pipe content).
    Verified with zlib (`-j 8`, `-j 16`) and then libpng (real GNU
    Libtool, a real dependency graph, ~40 compile/link steps, `-j 12`) --
    both build with zero jobserver warnings and pass their own real
    functional self-tests (`examplesh`'s compress/uncompress round trip;
    `pngtest`'s `libpng passes test`). `tools/crt-port-build.py`'s
    Windows-only `jobs = 1` special case is removed -- Windows now uses
    the same `os.cpu_count() or 2` default every other OS already used;
    `--jobs N` still overrides it for any single invocation. Any new
    Windows parallel-build problem found from here on gets its own fresh
    entry, not appended into this one.
  - Harden `waitpid()` and the child registry for many live children,
    configure-script subprocess bursts, and pipeline teardown.
  - Keep the mksh child-spec path (external commands, `cmd | cmd`,
    builtin-to-external pipelines, `cmd > file`/`cmd < file`, fd 3+
    redirection, exit-status propagation, multi-child/pipeline teardown)
    stable under real configure workloads.
  - Audit the mksh subshell status quirk exposed by commands shaped like
    `(command || true) >/dev/null 2>&1`.

- **Windows symlink/delete timing verification.** `readlink()`/`lstat()`/
  `symlink()` are all real and substantially better-verified now (see
  `HISTORY.md`'s dangling-symlink `lstat()` fix and `readlink()`
  truncation fix), but one open item remains: the intermittent
  `make install` `ln: ... File exists` failure on libtool-generated
  header/lib alias symlinks, seen when rebuilding a port whose install
  directory already has a valid symlink from a prior successful run (see
  the libpng `shared-pass` entry in `HISTORY.md`). An isolated, minimal
  repro succeeded cleanly every time, so this looks like same-session
  Windows delete-pending/handle-timing noise rather than a real toybox/
  CRT `rm`-on-symlink bug -- needs reproduction from a genuinely cold
  `out/` directory to confirm either way.
  - **New data point, not yet conclusive**: a related, unexplained
    `Error 5` (`ERROR_ACCESS_DENIED`, no message) hit `make install`
    twice on two different targets (`install-man5` on aarch64,
    `install-binSCRIPTS` on x86_64 -- see `HISTORY.md`'s `id`/`xargs`
    entry), neither reproducing on an immediate retry. The retry that
    stayed clean happened to run right after the Windows Defender
    process/folder exclusions documented in `README.md` were applied.
    Consistent with the working "Windows delete-pending/handle-timing
    noise" theory (Defender real-time scanning holding a file handle
    open just long enough to collide with `make install`'s own rapid
    create/delete sequence), but not proven -- multiple other rebuilds
    were running concurrently at the time, confounding a clean
    before/after comparison. Worth specifically re-testing from a cold
    `out/` directory with Defender exclusions active, to see if the
    intermittent `ln: ... File exists` failure above also stops
    reproducing.

- **Recipe/port status upkeep.** Keep `make`/`zlib`/`libpng`/`libffi`
  recipe statuses (`porting/recipes/*.json`, `docs/porting_status.md`)
  current as each host is rerun.

- **Standing porting-loop discipline**, not a task list:
  1. expose the missing header/type/macro/symbol/behavior with upstream
     source;
  2. check Android Bionic public headers, source, ABI, and errno policy;
  3. extend CRT/PAL/sysroot rather than patching upstream first;
  4. record host-specific policy differences in `docs/`.

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

## planed

- libffi's Windows build succeeds and its core features (`ffi_call`,
  closures) work correctly in isolation, but has one remaining,
  well-characterized bug (a callee-saved-register corruption across
  `ffi_call()` at `-O1`/`-O2`, see `HISTORY.md` and
  `porting/recipes/libffi.json`'s own notes for the full trail) still
  open -- would need a real debugger session to fully root-cause.
- Parallel `make -jN` on Windows is no longer an open research item -- it's
  enabled by default now (see "in progressing" above and `HISTORY.md`).
  Add a permanent regression test for the fixed bug (fd_snapshot dropping
  `FD_CLOEXEC` dup2 sources; `fstat()` destructively reading pipe content)
  so it can't silently regress, covering: inherited pipe fds across
  `posix_spawn()`, jobserver-style pipe transport, concurrent child wait,
  and close-on-exec filtering under load.
- Expand Windows shell smoke tests:
  - fd 3+ redirection inside mksh;
  - grouped commands;
  - background commands where non-interactive semantics are clear;
  - configure-script patterns involving subshells and redirections.
- Decide and document the minimal Windows console process-group policy needed
  for interactive mksh:
  - Ctrl-C / Ctrl-Break delivery;
  - foreground process group approximation;
  - stopped-child status policy.
- Add virtual rootfs files narrowly as porting workloads require them:
  - `/proc/mounts`;
  - `/proc/self/status`;
  - `/proc/self/cmdline`;
  - `/proc/self/environ`;
  - `/proc/stat`;
  - `/dev/zero`;
  - `/dev/random`;
  - `/dev/urandom`.
- Expand toybox applets only when the backing Bionic-compatible CRT/PAL
  surface exists. `which`/`readlink`/`stat`/`touch`/`id`/`xargs` are done
  (see `HISTORY.md`) -- next candidates would come from auditing the
  remaining disabled applets for LLP64 pointer-width safety (see "in
  progressing").
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
- Windows `fork()` itself is no longer an open research item -- both
  Windows architectures have a working, verified Cygwin/MSYS-style
  memory-copy `fork()` (`docs/windows_fork_emulation.md`; see `HISTORY.md`
  for the full settlement of the concerns originally listed here: saved
  register/context state, stack mapping/copy policy, writable segment
  policy, TLS reset, malloc/pthread/stdio/fd after-fork hooks,
  ASLR/base-address handling). Kept only as a pointer: any *new* Windows
  `fork()` problem discovered from here on should become its own fresh
  entry, not get appended here.
