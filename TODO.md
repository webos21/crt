# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## done

- Established `shell/` as a core CRT artifact area, not a third-party port
  recipe.
- Built `crt_tiny_sh`, Android `external/mksh`, and Android `external/toybox`
  through CMake.
- Generated an Android-like rootfs with `/system/bin`, `/bin`, `/usr/bin`,
  `/tmp`, `/dev`, and `/proc/self`.
- Installed mksh and the minimal configure-oriented toybox applet set into the
  rootfs.
- Kept POSIX hosts on symlink aliases and Windows on copy-based `.exe` aliases.
- Added the first Windows shell child process contract:
  - cwd/rootfs/env propagation;
  - fd snapshot export/import;
  - file actions and close-on-exec filtering;
  - child registry integration;
  - `waitpid()` coverage;
  - socket fd transport through `WSADuplicateSocketA()`.
- Documented real Windows `fork()` as a long-term PAL research tranche instead
  of blocking the mksh/toybox milestone on full fork emulation.
- Made Windows rootfs mksh run single external commands, external-command
  pipelines, builtin-to-external pipelines, and basic input/output redirection
  against CRT toybox applets.
- Fixed Windows toybox `ls -al` directory entries that showed `?` metadata for
  `.` and `..`.
- Recorded toybox LP64/LLP64 patches in `shell/toybox/PATCHES.md`; active fixes
  cover `dirtree.extra`, `ls`, the common option parser, Windows applet path
  lookup, and known active pointer-tagging paths.
- Kept zlib aligned with Android's model: zlib is a separate `libz`
  sysroot/runtime library surface, not part of Bionic libc.
- Confirmed AOSP does not carry GNU make under `platform/external`; Android
  carries make source under `toolchain/make` and prebuilts under
  `platform/prebuilts/build-tools`.
- Added `porting/recipes/make.json` and built Android `toolchain/make` as the
  first CRT-owned bootstrap build tool.
- Taught configure recipes to prefer `PORT_PREFIX/bin/make` before falling back
  to host make.
- Unified configure recipe launching through rootfs mksh for Windows, macOS,
  and Linux target flows.
- Made Windows CRT-shell configure recipes run `make -j 1` and pass
  `SHELL=/system/bin/mksh` so recipe commands stay on the project shell/process
  path.
- Completed Windows x86_64 zlib `./configure --static && make && make install`
  through rootfs mksh and CRT-built make.
- Set the zlib recipe to undefine Windows compiler predefines so upstream zlib
  stays on its generic POSIX path rather than selecting the Win32 `<io.h>`
  branch.
- Set zlib `RANLIB=true` because the optional zlib ranlib step is redundant for
  the LLVM archive path and exposed a Windows mksh subshell status quirk.
- Added Bionic/POSIX CRT surface exposed by make/zlib/shell work:
  - `alloca.h`;
  - `ar.h`;
  - `memrchr`;
  - `confstr`;
  - `_CS_PATH` / `_CS_V7_ENV`;
  - `ttyname`;
  - `getlogin`;
  - `eaccess`;
  - `bsd_signal`;
  - `EXIT_SUCCESS` / `EXIT_FAILURE`;
  - `putenv`;
  - `pselect`.
- Added or expanded regression tests for:
  - string memory helpers;
  - `confstr`/sysconf behavior;
  - `pselect`;
  - process signal helpers;
  - Windows fd snapshot and spawn attribute behavior.
- Updated the active status docs:
  - `docs/sysroot_ports.md`;
  - `docs/porting_status.md`;
  - `docs/shell_import.md`;
  - `docs/windows_fork_emulation.md`;
  - `shell/toybox/PATCHES.md`.
- Fixed the `port-rebuild-zlib` `make -j 10` deadlock: `sigaction()`/
  `sigprocmask()` previously only updated process-local bookkeeping with no
  real OS-level signal delivery, so GNU make's jobserver `pselect()` could
  never be interrupted by a real `SIGCHLD`. Added a per-OS
  `crt_signal_backend` (macOS: real `sigaction`/`sigprocmask` via a shared
  Mach-O export-trie helper now also reused by `libdl`; Linux: raw
  `rt_sigaction`/`rt_sigprocmask` syscalls plus an x86_64 restorer
  trampoline; Windows: honest no-op stub) and fixed a separate `pselect()`
  lost-wakeup race (`libc/src/poll.c`) where an already-pending signal was
  silently swallowed by the non-atomic mask-then-select sequence. Verified
  against the real `port-rebuild-zlib` `configure && make -j 10 && make
  install` end to end on macOS. See `docs/signal_delivery.md`.
- Fixed Windows aarch64 compile errors (`init_ntdll`/`fd_set_inherit_for_fork`
  unused-function under `-Werror`): both only backed the x86_64-only
  `RtlCloneUserProcess` fork path and were genuinely dead code on aarch64;
  guarded behind the same `#if defined(__x86_64__) || defined(_M_X64)`
  already used at their call site.
- Fixed 3 Windows aarch64 fork test failures (`fork_test`,
  `fork_signal_test`, `fork_runtime_reset_test`): only one of four
  `fork()`/`_Fork()` call sites treated Windows `ENOTSUP` as an expected,
  graceful pass; extended the same handling to the other three.
- Fixed 3 Windows aarch64 mksh rootfs ctest failures
  (`crt_mksh_rootfs_external_runs`/`_pipeline_runs`/`_command_substitution_runs`):
  root cause was a stale/missing `rootfs` build artifact, not a code bug --
  the `rootfs` CMake custom target had no `ALL` and nothing forced it to
  rebuild before ctest ran. Made `rootfs` part of `ALL` on Windows (the only
  host where any ctest entry depends on it); macOS/Linux keep it opt-in.
- Found and fixed a real mksh/CRT-shell-child-spec bug while investigating a
  separate, silent (`zero output, exit 1`) `port-rebuild-zlib` `./configure`
  failure on Windows aarch64: `MKSH_CRT_SHELL_CHILD_SPEC`'s `exchild()` fast
  path incorrectly ran `TPAREN` (subshells) in-process like `TCOM`, so a
  subshell's own redirection (e.g. `(cmd) 2>/dev/null`) permanently
  clobbered the interpreter's real stderr with nothing to restore it,
  silently swallowing every later error in the same script. Fixed by
  restricting the fast path to `TCOM` only (`shell/mksh/src/jobs.c`) --
  and found a second, independent copy of the same guard inside
  `execute()` itself (`shell/mksh/src/exec.c`), reached directly by
  `comsub()` (backtick/`$(...)` substitution) without ever going through
  `exchild()`, which is why the `jobs.c` fix alone did not change the
  observed behavior; fixed the same way. See
  `docs/windows_fork_emulation.md` for the full diagnosis. This does not make
  `zlib`'s `configure` pass on Windows aarch64 (still needs real `fork()`
  there), but turns the silent corruption into an honest `can't fork - try
  again` failure, and fixes a latent version of the same bug on Windows
  x86_64 (where real fork already exists).
- Made real `fork()` work on Windows aarch64 (`RtlCloneUserProcess` is
  exported there too; the previous x86_64-only guards in
  `libc/src/arch/windows/common/syscall.c` were simply untested
  assumptions) and fixed a genuine `fd_set_inherit_for_fork()` bug found
  along the way (fd 0/1/2 were never marked inheritable, breaking
  `2>&1` inside a forked subshell). This alone didn't make
  `configure`-driven builds work, though: `CreateProcessA()` (and, it
  turned out, `CreatePipe()`) both crash/fail when called from inside an
  unregistered `RtlCloneUserProcess` clone, because the clone never goes
  through CreateProcess's CSRSS registration handshake. Benchmarked the
  cost of routing spawns through `CreateProcessA` instead of raw fork
  (~1.2x, not the order-of-magnitude Cygwin reputation suggests),
  researched prior art (no documented CSRSS re-registration method
  exists; ruled out as too fragile), and built a "spawn broker": `fork()`
  stays untouched (still cheap `RtlCloneUserProcess`), but
  `__crt_sys_posix_spawn()` now detects when it is running inside an
  unregistered clone and, in that case, asks an always-running, never-
  cloned broker process (`libc/src/arch/windows/common/spawn_broker.c`,
  protocol in `libc/include/private/crt_spawn_broker.h`) to create the
  pipe and the real target process on its behalf, handing the resulting
  handles back via `DuplicateHandle`. Verified end to end on real Windows
  aarch64 hardware: `cmake --build --preset windows-host-ninja-debug
  --target port-rebuild-zlib` now completes zlib's full
  `configure && make && make install` with exit 0 (previously failed at
  `can't fork - try again`, then at a `CreateProcessA` crash, then at a
  `CreatePipe()` failure -- each fix exposing the next layer). Full
  `ctest` stays green (77/77) throughout. See
  `docs/windows_fork_emulation.md`, "Chosen Direction: Spawn Broker", for
  the full investigation, the benchmark numbers, the prior-art research,
  and the rejected alternatives (CSRSS re-registration, full Cygwin-style
  memory-copy `fork()`).

## in progressing

- **Retired the spawn broker; moving to a Cygwin/MSYS-style `fork()` instead.**
  The broker (see "done" above) fixed zlib and got libpng most of the way,
  but kept surfacing new structural failure modes of its own this session
  (orphaned `mksh.exe` processes, named-pipe races, I/O timeouts, a
  process-tree-reparenting attempt that regressed the working state and had
  to be reverted -- see the entry below). Decided to isolate it out of the
  active build rather than keep hardening it, and pursue the alternative
  recorded in `docs/windows_fork_emulation.md`'s "Rejected alternatives"
  section instead: a real Cygwin/MSYS-style `fork()` (`CreateProcessA` +
  `WriteProcessMemory` memory copy + `setjmp`/`longjmp` resume), which
  removes the "unregistered clone" problem at its root instead of working
  around it process-by-process.
  - **Phase A (done):** moved `spawn_broker.c`/`crt_spawn_broker.h` into
    `libc/src/arch/windows/legacy_spawn_broker/` (kept, not deleted, but
    excluded from `libc/CMakeLists.txt`'s `CRT_SYSCALL_FILE`); reverted the
    three `__crt_windows_is_unregistered_clone()` branches in
    `__crt_sys_open()`/`__crt_sys_pipe()`/`__crt_sys_posix_spawn()`
    (`libc/src/arch/windows/common/syscall.c`) back to their pre-broker
    direct-`CreateFileA`/`CreatePipe`/`CreateProcessA` form; removed the
    `CRT_SPAWN_BROKER_MODE` dispatch from `crt1.c`. Full `ctest` stays green
    (78/78) -- current test coverage does not exercise fork-then-spawn from
    inside a clone directly, so this is a safe mechanical revert. **Known,
    accepted regression:** until the new `fork()` lands, any real scenario
    that needs a forked clone to spawn a further process (e.g. a subshell
    inside `configure` running the compiler) will fail again the same way it
    did before the broker existed; `libpng`'s build (see below) is blocked
    on this.
  - **Phase B (done):** `/DYNAMICBASE:NO` turned out to be rejected by the
    linker on aarch64 (`lld-link: error: /dynamicbase:no is not compatible
    with arm64` -- ARM64 PE images must always be relocatable, so there is
    no link-time way to disable image ASLR on this architecture at all).
    Verified instead with `STARTUPINFOEXA` +
    `UpdateProcThreadAttribute(..., PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
    ...)` setting `PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF
    | _HIGH_ENTROPY_ASLR_ALWAYS_OFF` at `CreateProcessA` time (a per-process
    creation attribute, not a PE image characteristic): a probe executable
    spawned this way landed its first `malloc()` and a stack-local variable
    at byte-identical addresses across 2 independent top-level launches x 20
    children each (42/42 matching); the image's own code address was
    already deterministic on this system even *without* the mitigation
    policy, so only heap/stack needed it. This is architecture-independent
    in principle (a process-creation attribute, not a link flag), though
    only verified on aarch64 so far. **The Phase C address-matching
    assumption is confirmed feasible on real Windows aarch64 hardware.**
  - **Phase C (done, aarch64):** `__crt_sys_fork()` now dispatches to a new
    `__crt_windows_memcopy_fork()`
    (`libc/src/arch/windows/aarch64/fork_memcopy.c`) on aarch64; x86_64
    keeps the original `RtlCloneUserProcess` path untouched. `ctest` was at
    76/78 immediately after this landed (two known-open gaps in the
    startup self-relaunch's fd inheritance, since fixed -- see the
    "fd-inheritance gap" update below). Several things came up that the
    design write-up above didn't anticipate:
    - `malloc.c`'s `block_header` split chain is not the same thing as the
      underlying OS `mmap()`/`VirtualAlloc()` region boundaries -- a single
      64KB chunk gets subdivided into several non-64KB-aligned sub-blocks
      once anything allocates from it, and `VirtualAllocEx()` requires an
      explicit `lpAddress` to be allocation-granularity-aligned. Fixed by
      adding a separate, dedicated OS-region tracking table in `malloc.c`
      (`__crt_malloc_os_region_count()`/`_base()`/`_size()`, populated only
      in `append_chunk()`), independent of the block-split bookkeeping.
    - The child's `CONTEXT.Pc` was originally redirected to a small
      trampoline function that itself called `longjmp()` on a `jmp_buf`
      copied into the child's memory (matching the original design). This
      reproducibly crashed (`STATUS_ACCESS_VIOLATION`, DEP/execute
      violation, inside `longjmp()`'s own restore sequence) -- a
      `ReadProcessMemory()` readback taken immediately before
      `ResumeThread()` confirmed the copied `jmp_buf` bytes were correct
      at that point, but had become all-zero by the time the child's own
      code tried to read them (bisected with `WaitForDebugEvent()`/
      `ContinueDebugEvent()`, since in-process exception handlers weren't
      reliable here; exact mechanism not fully isolated). Fixed by
      skipping the trampoline/`longjmp()` indirection entirely: the parent
      already holds every register value the trampoline would have read
      from memory, so it writes them straight into the child's `CONTEXT`
      itself via `SetThreadContext()` and never asks the child to read
      resume state back out of memory at all -- a more robust design, not
      just a workaround.
    - Also newly required: fork()-capable processes self-relaunch once at
      startup under the same mitigation policy
      (`libc/src/arch/windows/aarch64/fork_capable_relaunch.c`,
      `__crt_windows_ensure_fork_capable_relaunch()`) -- Phase B only
      verified that *children spawned under the policy* get deterministic
      addresses, not the *original process itself* (ordinary ASLR), so a
      later `fork()` call's own addresses would never have matched a
      mitigated child's without this. This went through two more
      revisions: the "did I already relaunch" check moved from an
      inheritable env var marker (wrong -- survives `execve()` even when
      the new process image is not actually mitigated) to querying
      `GetProcessMitigationPolicy()` directly; and the relaunch itself
      moved from unconditional (every Windows aarch64 process, which broke
      external-command stdio entirely -- see docs) to opt-in per target via
      a weak symbol, linked only into `crt_mksh` and the ctest suite.
    - See `docs/windows_fork_emulation.md`, "Spawn Broker Retired", for the
      full account including known limitations (only the calling thread's
      stack survives into the child, per POSIX; no guard-page preservation
      past what was committed at fork time; x86_64 not yet ported).
    - **Update: fd-inheritance gap across the self-relaunch, fixed.** The
      startup self-relaunch's `CreateProcessA()` hop only forwarded the 3
      standard handles, so any other fd the process itself had received
      (e.g. via `posix_spawn_file_actions_adddup2()`) was silently lost --
      the 2 remaining `ctest` failures (`shell_smoke_test`,
      `windows_fd_snapshot_test`) both exercised exactly this. Fixed by
      reusing the exact mechanism `__crt_sys_posix_spawn()` itself already
      uses for every ordinary spawn (explicit `DuplicateHandle()`-into-
      child + a suspended-child-and-pipe handoff, not bare Windows handle
      inheritance) for the relaunch hop too: three new functions in
      `syscall.c` (`__crt_windows_fd_snapshot_relaunch_begin/_finish/
      _abort()`, declared in `private/crt_fd_table.h`) let
      `fork_capable_relaunch.c` export/duplicate/hand off the current fd
      table without needing its own `fd_table` access. This also required
      reordering `crt1.c` so `__crt_child_bootstrap()` (which imports an
      *incoming* fd snapshot into this process's own fd table) runs
      *before* the relaunch check, not after -- otherwise the relaunch's
      own export would only ever see the default fd 0/1/2 regardless of
      what this process itself had just received. `ctest` is back to
      78/78. As a side effect, this also fixed the previously-unexplained
      `mksh -c "exec mksh -c '...'"` recursive hang (verified stable across
      repeated runs and 3-deep nesting) -- it was the same gap: the
      exec'd-into relaunch was losing the inherited pipe/fd state the
      outer mksh needed.

- Started running libpng's real `configure && make && make install` through
  project-owned mksh/make on Windows aarch64 (`port-rebuild-libpng`, depends
  on zlib already installed to `PORT_PREFIX`). Not passing yet, but found and
  fixed three real, independent CRT/mksh bugs along the way, each verified
  against the full `ctest` suite (78/78 at the time) with no regressions:
  - **`regcomp()`/`regexec()` never implemented capture groups**
    (`libc/src/regex.c`): `\( \)` (BRE) / `( )` (ERE) were either treated as
    literal parenthesis characters to match (BRE) or silently miscounted
    into `re_nsub` (both modes), so `regmatch_t` subexpression bounds were
    never populated. This broke the extremely common autoconf idiom `expr
    "x$opt" : 'x--[^-]*-\(.*\)'` used to parse `--enable-X`/`--disable-X`/
    `--prefix=VALUE` style flags -- `./configure --disable-shared
    --enable-static --prefix=...` was silently corrupted into `--disable-0
    --enable-0 --prefix=0` before libpng's (autoconf-generated, unlike
    zlib's hand-written) `configure` ever got going. Rewrote the matcher to
    track real capture groups through backtracking (see `regex_test.c`),
    and fixed `re_nsub` counting to be BRE/ERE-mode-aware instead of always
    counting unescaped `(`.
  - **mksh never recognized Windows drive-letter paths as absolute**
    (`shell/mksh/src/sh.h`, `mksh_abspath()`): mksh already ships a full
    DOS-path-aware implementation behind `MKSH_DOSPATH`, but that flag also
    switches `PATH`/`CDPATH` to `;`-separated, which conflicts with this
    project's deliberate `:`-separated rootfs `PATH` convention
    (`/system/bin:/bin:/usr/bin`, see `tools/crt-port-build.py`). Added a
    narrower, CRT-owned `MKSH_CRT_WINPATH` define (alongside the existing
    `MKSH_CRT_ALLOW_LLP64`/`MKSH_CRT_SHELL_CHILD_SPEC`) that only patches
    `mksh_abspath`/`mksh_cdirsep`/`mksh_sdirsep` to recognize `X:\`/`X:/`
    and `\` as well as `/`, without touching `MKSH_PATHSEPC`. Without this,
    `cd "$(pwd)"` (autoconf's own `ac_pwd`/`ls -di .` working-directory
    sanity check) silently treated the Windows-native absolute path
    `getcwd()` returns as *relative*, doubling it into `<dir>/<dir>` and
    failing with `configure: error: working directory cannot be
    determined`.
  - **Generic `pipe()` has the same CreateProcessA-adjacent failure as the
    spawn broker's own bootstrap pipe, but was never routed through the
    broker** (`libc/src/arch/windows/common/syscall.c`,
    `spawn_broker.c`/`crt_spawn_broker.h`): `__crt_sys_pipe()` called
    `CreatePipe()` unconditionally, with no check for
    `__crt_windows_is_unregistered_clone()`. mksh forks a real subshell
    (`RtlCloneUserProcess` clone) for every command substitution/pipeline,
    and any further pipe needed *inside* that clone (nested command
    substitution, `cmd1 | cmd2`) hit exactly the already-diagnosed
    `CreatePipe()`-fails-in-an-unregistered-clone bug -- just reached via
    plain shell usage instead of `posix_spawn()`. Extended the broker
    protocol with a `want_plain_pipe` request (broker creates a pipe
    locally and `DuplicateHandle`s *both* ends back into the client,
    instead of attaching one end to a spawned target) and made
    `__crt_sys_pipe()` route through it when inside an unregistered clone.
  - Also found and fixed a fourth real bug along the way, confirmed
    necessary but **not sufficient on its own** -- see "still open" below:
    `ERROR_INVALID_NAME` (Windows error 123, returned for any path
    containing a character Windows never allows in a real filename, e.g.
    `*`) fell through `map_windows_error()`'s default case to `EIO`
    instead of `ENOENT`. Autoconf's own exit-trap cleanup runs `rm -f core
    *.core core.conftest.*`; when the glob doesn't match anything, mksh
    passes the literal pattern through (normal, expected shell behavior),
    and a literal `*` can never exist as a real Windows filename, so
    `ENOENT` is the semantically correct mapping -- and it is exactly what
    toybox's `rm -f` checks for to stay silent
    (`shell/toybox/src/toys/posix/rm.c:110`, `errno == ENOENT`). Fixed in
    `map_windows_error()`. Verified against the full `ctest` suite (78/78).
  - **Broker named-pipe server had two independent races**, found via
    `set -x` tracing and confirmed with per-PID debug logging (removed
    once fixed): (1) the server created the *next* waiting pipe instance
    only *after* servicing the current request, leaving a real window
    (the full service time, e.g. a `CreateProcessA` call, plus
    disconnect/close/recreate) with zero pending instances -- any client
    racing to connect during that window saw `ERROR_FILE_NOT_FOUND`,
    observed as mksh's `can't create pipe - try again` failing
    `checking whether build environment is sane`. Fixed by creating the
    next instance immediately after accepting the current connection,
    before servicing it. (2) `WriteFile()` returning success only means
    the response bytes reached the kernel's pipe buffer, not that the
    client read them; the server's `DisconnectNamedPipe()` tore the
    connection down immediately regardless, intermittently losing the
    response and failing the client's `read_exact()` with
    `ERROR_PIPE_NOT_CONNECTED` (233). Fixed by calling
    `FlushFileBuffers()` (blocks until the client has read everything)
    before disconnecting. Verified with 50+ repeated runs of the exact
    failing sanity-check idiom with no further "can't create pipe"
    failures, and the full `ctest` suite (78/78).
  - **The broker client's `read_exact()`/`write_exact()` had no
    timeout**, discovered while investigating orphaned `mksh.exe`
    processes left running (0% CPU, blocked, no ancestor process left
    alive) after a build had already finished. If the broker ever fails
    to respond for any reason, the mksh subshell that asked it for a
    pipe/spawn blocks in `ReadFile`/`WriteFile` forever -- Windows does
    not cascade-kill it, so it just sits there indefinitely, invisible
    in any log because it never crashes. Rewrote the broker's pipe
    handles (both client and server ends) to use `FILE_FLAG_OVERLAPPED`
    and added a bounded (20s) wait via `WaitForSingleObject` +
    `CancelIoEx` on timeout. This did **not** fully eliminate the
    orphaned-process symptom on its own (see the `TPAREN` bug just
    below, which was the real remaining cause in the cases actually
    investigated) but is an independently correct hardening: no broker
    I/O call can block a caller forever again, regardless of cause.
  - **The real bug behind the remaining orphaned-subshell/exit-127
    symptoms: a subshell reached without `XFORK` already set skipped
    real process isolation entirely** (`shell/mksh/src/exec.c`,
    `execute()`'s top entry check). A `TPAREN` (`(...)`) is supposed to
    always get a real fork via `exchild()`, but the entry check only
    forks when the caller already passed `XFORK` -- and `TLIST`'s
    handling of the *last* item in a sequence (`case TLIST` in the same
    file) passes `flags` straight through unchanged, with no `XFORK`.
    So `{ cmd1; cmd2; (subshell); }` -- a `TLIST` whose last item is a
    `TPAREN` -- reaches the entry check with no `XFORK`, skips it
    entirely, and falls through to the `case TPAREN:` handler, which
    recurses into the subshell's own content with `XFORK` freshly
    added. If that content is a single `TCOM`, the *same* entry check's
    existing `&& t->type != TCOM` term (added for a different, already-
    fixed bug -- see "Found and fixed a real mksh/CRT-shell-child-spec
    bug..." above) then blocks the fork for it too, since nothing there
    distinguishes "a TCOM already isolated by a real fork" from "a TCOM
    that IS the not-yet-forked subshell's own content." Concretely:
    `(exit $ac_status)` as the last statement of a `{ ...; }` group --
    exactly the idiom automake's generated `configure` uses to probe for
    optional tools like `tar` -- ran `exit` in the *interpreter itself*
    instead of a subshell, killing the whole `./configure` script
    instead of just that one probe attempt. This is what was actually
    behind the `checking how to create a ustar tar archive` exit-127
    failure recorded below (the pipe-broker races above are real,
    independently-fixed bugs, but were not sufficient to explain this
    one). Fixed by making the entry check fork on `t->type == TPAREN`
    unconditionally, regardless of whether `XFORK` was already set --
    unlike every other node type, "just run me in this interpreter" is
    never correct for a subshell. Verified with a standalone repro
    (`(exit 99)` as the last statement of a `{ }` group inside a loop,
    ran 3 iterations correctly instead of dying on the first) and the
    full `ctest` suite (78/78); `checking how to create a ustar tar
    archive` now correctly resolves to `none` and configure proceeds
    well past it (through `checking for gcc`) instead of aborting.
  - **Update (post spawn-broker retirement / memory-copy fork()):** the
    `can't create conftest.err: Bad file descriptor` failure this section
    used to describe was a symptom of the spawn broker, which no longer
    exists (see "Phase C" above). With the broker gone and memory-copy
    `fork()` in its place, `configure` now gets *substantially* further --
    all the way past `checking whether the C compiler works... yes` (the
    exact step that used to fail) and through `checking build system
    type`/`checking host system type` -- before hanging (not crashing:
    confirmed genuinely stuck via the CPU-delta technique, unchanged CPU
    across a 10s window) at the very next step, `checking for a sed that
    does not truncate output`. This is autoconf's own self-test that
    builds a `sed` script by repeatedly doubling a fixed pattern string,
    then pipes the whole thing through `sed` to find the length where
    truncation starts. **Root-caused and fixed.** Re-ran with
    `CRT_PORT_SHELL_XTRACE=1` and caught the exact stuck command via a
    live `Monitor` on the trace log: `echo "$ac_script" | sed 99q
    >conftest.sed`, where `$ac_script` is an ~11 KB doubled string. `echo`
    is a shell builtin; mksh's `MKSH_CRT_SHELL_CHILD_SPEC` Windows port
    (`shell/mksh/src/jobs.c`'s `exchild()`) skips a real `fork()` for a
    `TCOM` pipeline stage to avoid this platform's expensive memory-copy
    `fork()` when it turns out to be an external command -- but when the
    stage is a *builtin* instead, it runs synchronously in-process with no
    concurrent reader forked yet, and its `write()` into the pipe
    (`CreatePipe()`'s default buffer, ~4096 bytes) blocks forever once it
    exceeds the buffer. Binary-searched the exact threshold with a minimal
    `echo "$s" | wc -c` reproduction: 4051 bytes OK, 4101+ hangs
    indefinitely (confirmed via `timeout`, not just CPU-delta). Fixed by
    giving every `CreatePipe()` call in `syscall.c` (the generic `pipe()`
    syscall, the posix_spawn() fd-snapshot bootstrap pipe, and the
    fork-capable self-relaunch's fd handoff added just above) an explicit
    4 MiB buffer (`CRT_PIPE_BUFFER_SIZE`) instead of the system default --
    the latter two share the exact same synchronous-write-before-resume
    shape and would have hit the identical deadlock for a large enough fd
    table/snapshot, just not yet observed in practice. Verified: the
    isolated repro now succeeds well past the old threshold; `ctest` stays
    at 78/78; a full libpng `configure` re-run sails straight through the
    sed self-test and `checking for grep that handles long lines and -e`,
    reaching a *new*, much later, and non-hanging stopping point: `checking
    for egrep... configure: error: no acceptable egrep could be found` --
    `egrep`/`fgrep` are simply missing from `tools/create_rootfs.py`'s
    `TOYBOX_APPLETS` alias list (toybox's `grep.c` natively supports both
    as `OLDTOY` aliases of `grep`; just need to be added to the list and
    have their `USE_EGREP`/`USE_FGREP` config macros enabled in this
    project's hand-picked toybox build). See
    `docs/windows_fork_emulation.md`, "Windows Pipe Buffer Size", for the
    full writeup.
  - **Update: egrep/fgrep alias, ERE alternation, and pipe-lseek, all
    fixed; configure now blocked on a plain missing `ld`.** Three more
    real, independent bugs found and fixed chasing `checking for
    egrep`/`fgrep`, in order:
    1. `egrep`/`fgrep` weren't just missing from `tools/create_rootfs.py`'s
       `TOYBOX_APPLETS` alias list -- this project also doesn't use
       toybox's Kconfig `.config`; it hand-lists every enabled applet in
       `shell/toybox/crt/generated/newtoys.h`, which had no `OLDTOY(egrep,
       grep, ...)`/`OLDTOY(fgrep, grep, ...)` entries at all (toybox's own
       `grep.c` already supports both natively). Added both to
       `newtoys.h` and to `TOYBOX_APPLETS`.
    2. `grep -E 'bar|baz'` didn't match either alternative -- `|` was
       matched as a literal character. `libc/src/regex.c` was a 370-line
       hand-rolled backtracking matcher with **no alternation support at
       all** (not a regression; it was simply never implemented). Per
       explicit instruction, replaced it wholesale with the real Bionic/
       NetBSD Henry-Spencer strip-VM regex engine (`libc/src/regex/`,
       ported from `libc/upstream-netbsd/lib/libc/regex/` on Bionic
       `main`) -- full POSIX BRE/ERE, backreferences, bounded repetition,
       POSIX character classes, opt-in GNU BRE extensions. See
       `third_party/bionic/README.md`'s "Regex Tranche" for the full file
       list and adaptation notes (the interesting one: this project's real
       `wint_t` is `unsigned short`, which breaks the engine's negative
       sentinel comparisons, so the NLS/real-wide-char path is
       deliberately left off in favor of utils.h's own signed-`short`
       fallback -- see `libc/src/regex/netbsd-compat.h`'s top comment).
       Also added `reallocarray()` (`libc/src/reallocarray.c`) and
       `MB_LEN_MAX` (`include/limits.h`), both needed by the ported
       engine and missing from this libc before now.
    3. Even with alternation working, `grep`/`egrep`/`fgrep` still matched
       *nothing at all*, on *any* pattern, but only when reading from a
       **pipe** (a real file argument worked fine) -- root-caused to
       toybox `grep.c`'s "only run binary-file sniffing on lseekable fds"
       guard (`!lseek(fd, 0, SEEK_CUR)`): this project's Windows
       `__crt_sys_lseek()` called `SetFilePointerEx()` on whatever handle
       it was given with no check for whether it was actually seekable,
       so the guard's intended skip-on-pipe behavior silently didn't
       trigger -- grep's binary-sniffing peek-and-rewind ran on piped
       stdin too, consumed the pipe's data during the peek, and (since a
       pipe can't be rewound) never got it back, so the real read loop
       that followed started from an already-drained pipe. Fixed by
       checking `GetFileType(handle) == FILE_TYPE_PIPE` up front and
       returning `-ESPIPE`, matching POSIX `lseek(2)` on a pipe/FIFO.
       Regression-covered in `tests/fd_errno_test.c`.

    All three verified together: `ctest` 78/78 (plus new `regex_test.c`
    coverage for alternation/bounded-repetition/backreferences/POSIX
    classes/case-insensitivity, and the `fd_errno_test.c` lseek-on-pipe
    check); a full libpng `configure` re-run now sails through `checking
    for egrep`/`checking for fgrep` (`... /system/bin/grep -E` / `-F`) and
    reaches yet another new, later, ordinary (non-hanging) stopping point:
    `configure: error: no acceptable ld found in $PATH` -- this project's
    rootfs has no standalone `ld` binary at all (it drives `lld-link`/
    `crt-cc` directly, never a bare `ld`); next step is likely either
    aliasing one or making libpng's `configure` accept the existing
    toolchain wrapper instead. Not yet investigated.
  - **Update: `ld` not found, fixed.** `crt-cc` already links via
    `-fuse-ld=lld` (`tools/crt-cc`), so `ld.lld.exe` (shipped by the LLVM
    install) is this project's real linker backend already -- it just
    wasn't reachable under any name/location libtool's `AC_PROG_LD`
    ("checking for non-GNU ld") search would find, because native-Windows
    `--use-crt-shell` configure runs with `PATH` hard-restricted to this
    project's own rootfs (`/system/bin:/bin:/usr/bin`), unlike macOS/
    Linux, which append the *host* PATH and so already have a real system
    `ld` there (Xcode CLT / binutils) -- this was a Windows-only gap.
    Fixed in `tools/crt-port-build.py`'s `make_env()`: pre-set `$LD` to
    `ld.lld.exe`'s real path (found the same way `CRT_HOST_CC`/
    `CRT_HOST_CXX` already are, via `find_windows_host_tool()`, wrapped
    with the same `CRT_SPAWN_NATIVE_WINDOWS=1` prefix AR/RANLIB/STRIP
    already use) -- autoconf/libtool only search `PATH` for `ld` when
    `$LD` isn't already set, so this skips the broken search entirely.
    Verified: `checking for non-GNU ld... ...ld.lld.exe`,
    `checking if the linker (...) is GNU ld... no` (ld.lld's `-v` banner
    says "compatible with GNU linkers", not literally "GNU", so libtool
    correctly treats it as non-GNU-but-compatible), and configure sails
    through the entire libtool linker/shared-library-support detection
    phase (`checking whether ... linker ... supports shared libraries...
    yes`, ranlib/strip detection, PIC flags) to a new, much later, and
    completely different next blocker: `checking if awk () works...
    inaccessible or not found` / `configure: error: ... no` -- this
    project's rootfs has no `awk` at all yet (`checking for gawk/mawk/
    nawk/awk... no` earlier in the same log; toybox's own `awk.c` is a
    `pending`, not-yet-enabled applet).
  - **Update: real AWK ported and working.** Per explicit direction,
    ported Brian Kernighan's reference `onetrueawk` (NetBSD/many BSDs'
    own system awk) into `shell/awk/`, built with this project's own CRT
    like `mksh`/`toybox` -- deliberately *not* solved by pointing
    configure at a host-installed awk (e.g. Git for Windows' bundled
    `gawk.exe`), which would be an uncontrolled, per-machine, different-
    runtime dependency breaking the same self-containment principle that
    justified building `mksh`/`toybox` in the first place. See
    `shell/awk/README.md` and `import_manifest.json` for the full
    writeup; summary:
    - Installed `win_flex_bison` (via `winget`) to generate
      `awkgram.tab.c`/`.h` from upstream's `awkgram.y`, and built/ran
      upstream's own `maketab.c` (as a native host tool, via this
      project's own `crt-cc` against its own sysroot) to generate
      `proctab.c`. Both generated outputs are vendored as pristine,
      checked-in files (matching how `shell/toybox/crt/generated/*.h`
      are already vendored rather than regenerated at build time) -- this
      project's CMake build gained no new bison/yacc dependency.
    - One adaptation to upstream source: `parse.c`'s `ptoi()`/`itonp()`
      pointer-smuggling helpers cast through `long`, truncating on
      Windows LLP64; changed to `intptr_t`.
    - Filled several real, general (not awk-specific) CRT/libm gaps found
      compiling and then actually *running* awk programs: `<stdnoreturn.h>`
      (missing entirely), `atan2()`/`atan()` (FreeBSD msun, a separate
      upstream from the regex import), `system()`, `rand()`/`srand()`/
      `random()`/`srandom()` (a rand48-family LCG, not a literal port of
      BSD's own proprietary `random()` -- POSIX doesn't mandate a specific
      sequence), `SIGFPE` `FPE_*` `si_code` constants, and `popen()`/
      `pclose()`.
    - Found and fixed a real, general `printf`/`snprintf` bug this way
      too: `%g` with an *explicit* precision (`%.6g`, `%.30g`, ... --
      onetrueawk's own number-to-string conversion always uses `%.30g`)
      zero-padded the already-rendered digit string a second time (e.g.
      `%.6g` of `4.0` printed `"000004"`, not `"4"`) because
      `format_double_general()`/`format_long_double_general()`
      (`libc/src/printf.c`) forgot to clear `spec->precision_set` before
      their final `write_formatted()` call, unlike their fixed-point/
      exponential siblings which already did. No prior test had ever
      exercised `%g` with a non-default precision. Regression-covered in
      `tests/printf_test.c`.
    - Verified: `ctest` 79/79 (new `crt_awk_basic_runs` plus the printf
      regression cases above); manual smoke covering print/field-split/
      pattern-match/arrays/`printf`/`split`/`sqrt`/`atan2`/`rand`/`srand`/
      `getline`-from-`popen` all correct. A full libpng `configure`
      re-run now passes `checking for gawk... (cached) awk` /
      `checking if awk (awk) works... yes` and reaches yet another new,
      much later, and completely different next blocker: `checking for
      zlibVersion in -lz... no` / `configure: error: zlib not installed`
      -- despite zlib's own install stamp already being present in
      `PORT_PREFIX`; not yet investigated (a link/library-path issue,
      not a missing-tool issue like everything above).
  - Continued past that: `checking for zlibVersion in -lz... no` turned out
    to be a COFF-vs-Unix static-library-naming mismatch, not a missing
    build. This project's toolchain links via `clang -fuse-ld=lld`, whose
    lld-link backend resolves `-lfoo` to a file literally named `foo.lib`
    (matching this project's own CMake-built libs, e.g. `c.lib`/`m.lib`),
    but zlib's own autoconf/make install produces the Unix-conventional
    `libz.a`, which `-lz` could never find. Fixed generally (for all future
    Windows ports, not just zlib) with a new post-install step,
    `alias_unix_static_libs_for_windows_link()` in `tools/crt-port-build.py`:
    after every port build, copies each installed `libfoo.a` to `foo.lib`
    alongside it (skipped if `foo.lib` already exists). Verified: rebuilding
    zlib now also produces `z.lib`, and a full libpng `configure` re-run
    reaches `checking for zlibVersion in -lz... yes`.
  - Next blocker after that: `fatal error: 'windows.h' file not found`
    (`pngpriv.h:569`, guarded by
    `#if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)`). This
    project's sysroot has no real Windows SDK `<windows.h>` (its own Win32
    API surface is declared privately inside `libc/src/arch/windows/`, not
    exposed publicly). Nothing near that include in
    `pngpriv.h`/`png.c`/`pngerror.c` actually references a real Windows API
    symbol from it, so -- matching the exact technique zlib's own recipe
    already uses (`porting/recipes/zlib.json`'s `-U_WIN32` etc. `CFLAGS`) --
    fixed by adding the same `-U_WIN32 -U_WIN32_WCE -U__WIN32__ -UWIN32
    -U__NT__ -U_MSC_VER` `CFLAGS` to `porting/recipes/libpng.json` (plus the
    extra `-U__NT__`, since libpng's own Windows guard also checks that
    macro), keeping libpng on its generic/POSIX code paths.
  - Next blocker after that: `fatal error: 'arm_neon.h' file not found`
    (`pngrtran.c:26`). Root-caused to `tools/crt-cc`/`tools/crt-c++`
    hardcoding `resource_dir=""` on Windows, entirely skipping clang's own
    `-print-resource-dir` query -- meaning compiler-provided architecture-
    intrinsic headers (`arm_neon.h`, `immintrin.h`, ...) were never on the
    Windows include path at all. Also found a compounding latent bug while
    fixing this: both scripts build `common_flags`/`user_args`/`libs` as
    plain strings and pass them via *unquoted* expansion, relying on word-
    splitting to become separate argv entries -- which silently breaks any
    single value containing a space, and a stock Windows LLVM install's
    resource-dir is almost always under `"C:\Program Files\..."`. Fixed
    both scripts by always querying `resource_dir` (all platforms), and
    passing `-isystem "$resource_dir/include"` as its own separately-quoted
    argument pair directly on each `exec` line, instead of folding it into
    the unquoted `common_flags` string. Verified: `ctest` 79/79 still
    passing (these scripts compile everything in the Windows build, so this
    was the highest-stakes check of this whole chain); a standalone
    `crt-cc` compile of a translation unit including `<arm_neon.h>` now
    succeeds. Re-running the full libpng port build to find/fix whatever
    comes next is in progress.
  - Checked whether any of the fixes above are Windows-specific-only or
    could affect macOS/Linux (couldn't literally build for those hosts
    from this Windows aarch64 machine -- static review only): the
    `lseek()` fix lives entirely in `libc/src/arch/windows/common/
    syscall.c` (macOS/Linux use raw `syscall.S` kernel syscalls, which
    already return `ESPIPE` for pipes correctly, unaffected); the `LD`
    env var fix is explicitly `if target_os == "windows"`-scoped; the
    regex engine port, `reallocarray()`, `MB_LEN_MAX`, and the `egrep`/
    `fgrep` alias additions all live in shared, non-OS-forked files with
    no Windows-specific API references, so they apply identically (and
    identically safely, given `NLS` is deliberately never defined
    regardless of platform) across all three targets.

- Attempted to fix a real (if currently low-impact) gap in the spawn broker:
  every process it spawns shows up in Windows' own process tree as a child
  of the broker, not of the clone that logically requested it (flat instead
  of nested in Task Manager/Process Explorer/any future toybox `ps
  --forest`; `ps` itself is not enabled yet, `CFG_PS 0`). Tried the official
  `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` mechanism (same one `explorer.exe`
  uses for UAC-elevated children). Found and fixed one real bug along the
  way (inheritable handles are sourced from the *specified parent's* handle
  table with this attribute, not the actual `CreateProcessA` caller's --
  the fd-snapshot bootstrap pipe's read end had to move from
  "inheritable in the broker" to "duplicated into the client, inheritable
  there"), but hit a second, worse regression that was never fully
  isolated: spawned targets started failing entirely with
  `STATUS_DLL_INIT_FAILED` (a Windows loader-level failure, before the
  target's own `main()` ever runs), and -- the important part -- disabling
  just the reparenting attribute did **not** reliably fix it, meaning
  something in this line of changes broke the plain, non-reparented spawn
  path too, not just the reparented one. Since the actual acceptance test
  (`port-rebuild-zlib`'s real `configure`/`make`/`make install`) regressed
  back to failing, reverted both changed files
  (`libc/src/arch/windows/common/spawn_broker.c` and `.../syscall.c`) via
  `git checkout --` to the last known-good commit rather than ship a
  half-fixed state. Confirmed the revert restores the working state (zlib
  passes, `ctest` 77/77). Full blow-by-blow, what to try differently next
  time, and why job-object inheritance was ruled out as the cause: see
  `docs/windows_fork_emulation.md`, "Attempted And Reverted: Reparenting
  Spawned Processes To The Client".

- Verify the new Linux signal backend (`docs/signal_delivery.md`) on an
  actual Linux host; it is currently code-review-verified only, since this
  project's CMake presets refuse to cross-compile from macOS.
- Add a permanent regression test for the `fork()` + blocked-`SIGCHLD` +
  `pselect()` pattern used to verify the signal delivery fix.

- Keep the Windows mksh child-spec path stable for real configure workloads:
  - external command execution;
  - `cmd | cmd`;
  - builtin-to-external pipelines;
  - `cmd > file`;
  - `cmd < file`;
  - fd 3 and higher redirections;
  - child exit status propagation;
  - multi-child and pipeline teardown.
- Harden Windows `waitpid()` and the child registry for multiple live children,
  configure-script subprocess bursts, and pipeline cleanup.
- Track the current Windows make limitation:
  - serial make is the supported path;
  - parallel make/jobserver fd inheritance is not complete;
  - this should be fixed in CRT/PAL process/fd handling, not by returning to
    host make.
- Audit the Windows mksh subshell status quirk exposed by commands shaped like
  `(command || true) >/dev/null 2>&1`.
- Continue validating that `CRT_SPAWN_NATIVE_WINDOWS=1` remains a narrow
  launcher hint for native host tools such as LLVM `ar`, `ranlib`, and `strip`,
  not an inherited global mode for configure recipes.
- Keep make/zlib/libpng/libffi recipe statuses current as each host is rerun.
- Keep auditing disabled toybox applets for pointer-to-`long` LLP64 assumptions
  before enabling them.
- Keep `/dev/tty`, `/dev/console`, `isatty`, `tcgetattr`, `tcsetattr`, and
  `TIOCGWINSZ` behavior coherent enough for non-interactive shell and configure
  use.
- Preserve the porting loop discipline:
  1. expose the missing header/type/macro/symbol/behavior with upstream source;
  2. check Android Bionic public headers, source, ABI, and errno policy;
  3. extend CRT/PAL/sysroot rather than patching upstream first;
  4. record host-specific policy differences in `docs/`.

## planed

- Finish the libpng/libffi/SQLite configure/make/install pass on Windows
  (see "in progressing" above for the current libpng blocker); libffi and
  the SQLite follow-up build beyond the current amalgamation smoke have not
  been attempted yet.
- Re-run the same make/zlib/libpng/libffi recipe path on macOS and Linux to
  confirm the unified mksh+make flow across hosts.
- Add focused tests for parallel make prerequisites before enabling `make -jN`
  on Windows:
  - inherited pipe fds;
  - jobserver-style pipe transport;
  - concurrent child wait;
  - close-on-exec filtering under load.
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
- Expand toybox applets only when the backing Bionic-compatible CRT/PAL surface
  exists. Likely next applets:
  - `which`: add as a lightweight toybox applet for configure and shell
    usability; mksh has `whence`/`command -v`-style builtins, but `which`
    should be provided as an external applet.
  - `readlink`;
  - `stat`;
  - `touch`.
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
- Continue long-term Windows `fork()` research separately from the immediate
  mksh/toybox milestone:
  - saved register/context state;
  - stack mapping/copy policy;
  - writable runtime/data segment policy;
  - TLS/current-thread reset in the child;
  - malloc/pthread/stdio/fd after-fork reset hooks;
  - ASLR/base-address constraints or documented failure mode.
