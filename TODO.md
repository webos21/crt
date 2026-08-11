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
  - `xz` (liblzma): done on Linux (`shared-pass`) -- see
    `porting/recipes/xz.json`'s own notes and the new ".init_array" item
    below. Configure/build succeeded immediately; a real usage test
    surfaced and got a fix for a genuine, general CRT startup gap
    (`.init_array` never ran). A second apparent bug (a `malloc()`
    lock-state corruption once liblzma's real LZ match-finder allocates
    its dictionary/hash tables) turned out, after further investigation
    with a gdb watchpoint, to be a buffer overflow in this session's own
    throwaway verification program (an unchecked `snprintf()`-return-value
    accumulation, `size_t`-underflowing the remaining-buffer-size
    argument) -- not a CRT/PAL/liblzma bug at all. Fixed the test; real
    match-finder allocations up to ~512 MB and a full compress/decompress
    round trip at preset 9|EXTREME with CRC64 both work correctly on
    Linux, verified against static and shared (`ldd`-checked) builds.
    Linux is at `shared-pass`.
    Windows build needed two more fixes (both generalized into
    `tools/crt-port-build.py` since they're not liblzma-specific):
    `-U__MINGW32__` in the recipe's own CFLAGS (xz's `sysdefs.h`/
    `mythread.h` branch on a real mingw-w64 environment this sysroot
    doesn't fully replicate -- a missing `_mingw.h` and a `sigset_t`/
    `_sigset_t` remap that fights this project's own real `sigset_t`),
    and presetting `$RC` to `llvm-rc` plus overriding away the optional
    `liblzma_w32res.rc` version-info resource (`llvm-rc`'s GNU-windres
    `-i`/`-o` compatibility mode mis-invokes its own internal clang
    preprocessing step; not pursued further since the resource is purely
    cosmetic -- see `porting/recipes/xz.json`'s notes for the full
    trail, including a `crt-port-build.py` fix so a recipe's
    `make_args` override reaches `make install` too, not just `make`).
    Windows *builds* clean (`configure-pass`), but actually calling the
    library crashes -- confirmed directly (not just presumed): a trivial
    `lzma_version_string()` call works, but the first real
    `lzma_easy_buffer_encode()` call segfaults, on both the static and
    shared build, in the exact same "first real call into the CRC
    dispatcher" shape as the original Linux bug this port already found
    and fixed. Strong evidence it's the same `.init_array`-never-ran gap
    (see the entry below) reaching Windows too, but not confirmed with a
    debugger the way the Linux case was, and not fixed this session: PE
    constructors compile into a GNU/MinGW-style bare `.ctors` section
    (confirmed via `llvm-objdump -h` on a real constructor test, not
    MSVC's `.CRT$XCU` convention), which has no equivalent to ELF's
    automatic `__init_array_start`/`_end` boundary symbols -- MinGW-w64's
    own real crt0 relies on `-1`-sentinel objects (`crtbegin.o`/
    `crtend.o`) linked first/last to bound the merged section, a
    mechanism this project doesn't have and would need to add carefully
    (get the link-order sandwich wrong and it silently walks garbage)
    rather than as a quick follow-on to an already-long investigation.
    macOS still `pending` (no macOS hardware available this session).
    Next: `pcre2` (xz's Windows/macOS gaps trail along in the two
    entries below, not blocking further queue progress).

- **`.init_array`/`.fini_array` (ELF constructor/destructor) support**,
  found and fixed for Linux while porting xz/liblzma (see that recipe's
  own notes and `HISTORY.md`): this project's `crt1` startup never ran
  `__attribute__((constructor))`-registered functions (also what runs
  C++ global object constructors) for the executable entry point, on any
  of the three OSes -- invisible until now because every port that got
  this far linked shared, where the OS's own dynamic loader handles a
  `.so`'s own `.init_array` automatically. Fixed for Linux
  (`libc/src/arch/linux/common/init_fini_array.c`, wired into `crt1.S`/
  `exit.c`/`tools/crt-cc`/the CMake test and shell build paths); verified
  with gdb against the real liblzma bug that exposed it.
  - Windows and macOS still lack the equivalent fix -- and Windows is now
    *confirmed* (not just presumed) to need it too: the same xz/liblzma
    round-trip test that exposed and verified the Linux fix crashes on
    Windows in the identical shape (trivial API calls work, the first
    real call into liblzma's CRC dispatcher segfaults), on both the
    static and shared build. PE constructors compile into a bare
    `.ctors` section (GNU/MinGW convention, confirmed via `llvm-objdump
    -h` on a real `__attribute__((constructor))` test against this
    toolchain) rather than MSVC's `.CRT$XCU` family this TODO previously
    assumed -- so the fix shape is different from what's written below:
    `.ctors` has no ELF-style automatic linker-provided boundary
    symbols, so this needs the same `-1`-sentinel-object technique real
    mingw-w64 crt0 uses (a "begin" object linked immediately after
    crt1.o and an "end" object linked last, each contributing a `-1`
    value to the merged `.ctors` section, with the startup walker
    reading from the begin symbol until it hits `-1`) -- not attempted
    yet, since getting the link-order sandwich wrong would silently walk
    garbage instead of failing loudly. macOS needs to walk
    `__DATA,__mod_init_func` (Mach-O's own, separate convention). Neither
    implemented yet -- only matters for statically-linked
    constructor-reliant code, which is why nothing in this project's own
    test suite or any prior port ever exercised it before xz.
  - Also documented, not yet fixed: for an executable linked against
    *shared* libc (`c_shared`/`libc.so`) rather than static, `exit()`
    (which lives inside `libc.so` in that configuration) reaches
    `__crt_run_fini_array()` only via a weak symbol reference back into
    the executable's own `crt1` object -- and this toolchain doesn't pass
    `-rdynamic`/`--export-dynamic`, so that cross-DSO reference likely
    doesn't resolve. `.init_array` (construction) is unaffected by this
    gap, since `crt1.o` is always statically embedded into the
    executable regardless of how libc itself is linked.

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
