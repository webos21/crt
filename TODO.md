# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## note

- **Recipe/port status upkeep.** Keep recipe statuses
  (`porting/recipes/*.json`, `docs/porting_status.md`) current as each
  host is rerun.

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

Active threads, not a flat list of one-off items:

- **mbedtls's Windows DLL re-exports this project's own libc symbols,
  with no visibility control.** Confirmed real and NOT fixed (see
  `porting/recipes/mbedtls.json`'s own notes for the full trail):
  mbedtls's hand-rolled Windows `.dll` build statically embeds this
  project's entire libc and re-exports every symbol in it alongside its
  real public API. First seen as a link-time `ld.lld: error: duplicate
  symbol` (curl linking against both `libmbedcrypto.dll.a` and this
  project's own `c.lib`); that specific error stopped reproducing on a
  later attempt (still unexplained), but the underlying re-export was
  never fixed, and it resurfaced as a genuine runtime bug instead:
  `libcurl-4.dll`'s calls to `read()`/`__crt_sys_read()` silently
  resolved to `libmbedcrypto.dll`'s own embedded, stale copy (predating
  a real libc fix) instead of the current `c.lib`/`c.dll` -- root-caused
  with a live `lldb -p <pid>` attach. **Any future libc fix on Windows
  silently stops applying to anything linking mbedtls's DLL until
  mbedtls itself is also rebuilt** -- a real, general risk for every
  future Windows consumer of this port, not just curl, with no
  recipe-level workaround that actually closes it. Needs either real
  symbol-visibility control added to mbedtls's own Windows `.dll` build,
  or every consumer linking against mbedtls's static libraries
  specifically for their own shared builds instead of its DLL import
  library.

- **Windows shell/process stress hardening.** Real concurrency -- parallel
  `make -jN`, jobserver pipe fd handling, many live children in the
  registry at once, subshell/redirection edge cases -- was never actually
  exercised on Windows until this thread opened; every Windows port build
  had always run serial `make -j 1`.
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

## planned

- libffi's Windows build succeeds and its core features (`ffi_call`,
  closures) work correctly in isolation, but has one remaining,
  well-characterized bug (a callee-saved-register corruption across
  `ffi_call()` at `-O1`/`-O2`, see `HISTORY.md` and
  `porting/recipes/libffi.json`'s own notes for the full trail) still
  open -- would need a real debugger session to fully root-cause.
- Parallel `make -jN` on Windows is no longer an open research item -- it's
  enabled by default now (see `HISTORY.md`).
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
  remaining disabled applets for LLP64 pointer-width safety.
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
